use std::collections::HashMap;

use dockerfile_parser::{Dockerfile, Instruction};
use serde::Serialize;
use serde_json;
use std::io::Read;
use std::io::Write;

#[derive(Debug, Default, Serialize)]
struct Hermitfile {
    #[serde(rename = "FROM")]
    pub from: String,
    #[serde(rename = "LINK")]
    pub link: Vec<String>,
    #[serde(rename = "MAP")]
    pub map: Vec<String>,
    #[serde(rename = "NET")]
    pub net: Vec<String>,
    #[serde(rename = "ARGV")]
    pub argv: Vec<String>,
    #[serde(rename = "ENV")]
    pub env: Vec<String>,
    #[serde(rename = "ENTRYPOINT")]
    pub entrypoint: String,
    #[serde(rename = "ENV_PWD_IS_HOST_CWD")]
    pub uses_host_cwd: bool,
    #[serde(rename = "ENV_EXE_NAME_IS_HOST_EXE_NAME")]
    pub uses_host_exe_name: bool,
}

fn parse_hermitfile(hermitfile_path: &str) {
    let file = std::fs::read(&hermitfile_path).unwrap_or_default();
    let dockerfile = String::from_utf8(file).unwrap_or_default();
    let hf = Dockerfile::parse(&dockerfile).unwrap();

    let mut hermitfile = Hermitfile::default();
    let mut env_map: HashMap<String, String> = HashMap::new();

    for instruction in hf.instructions {
        match instruction {
            Instruction::From(ins) => {
                hermitfile.from = ins.image.content;
            }
            Instruction::Net(ins) => {
                if let Some(arr) = ins.expr.as_exec() {
                    hermitfile.net = arr.elements.iter().cloned().map(|s| s.content).collect();
                }
            }
            Instruction::Map(ins) => {
                if let Some(arr) = ins.expr.as_exec() {
                    hermitfile.map = arr.elements.iter().cloned().map(|ss| ss.content).collect()
                }
            }
            Instruction::Link(ins) => {
                if let Some(arr) = ins.expr.as_exec() {
                    hermitfile.link = arr.elements.iter().cloned().map(|s| s.content).collect();
                }
            }
            Instruction::Env(ins) => ins.vars.iter().for_each(|var| {
                let k = var.key.content.clone();
                let value = var
                    .value
                    .components
                    .first()
                    .expect("parsed ENV key with no value");

                if let dockerfile_parser::BreakableStringComponent::String(v) = value {
                    env_map.insert(k, v.content.clone());
                }
            }),
            Instruction::Entrypoint(ins) => {
                if let Some(v) = ins.expr.as_shell() {
                    if let dockerfile_parser::BreakableStringComponent::String(bs) = v
                        .components
                        .first()
                        .expect("ENTRYPOINT must have a function name as its argument.")
                    {
                        hermitfile.entrypoint = bs.to_string();
                    }
                }
            }
            Instruction::EnvPwdIsHostCwd(_) => hermitfile.uses_host_cwd = true,
            Instruction::EnvExeIsHostCwd(_) => hermitfile.uses_host_exe_name = true,
            _ => {}
        }
    }

    env_map.iter().for_each(|(k, v)| {
        hermitfile.env.push(format!("{}={}", k, v));
    });

    println!(
        "{}",
        serde_json::to_string_pretty(&hermitfile).expect("json serialized")
    );

    // load executable to use as the hermit
    let input_exe_name = std::env::var("EXE_NAME").unwrap();
    let (input_exe, input_perms) = {
        let input_exe_size: usize = {
            let input_exe_file = std::fs::File::open(&input_exe_name).unwrap();
            let mut input_exe_zip = zip::ZipArchive::new(input_exe_file).unwrap();
            // HACK .offset() doesn't appear to work
            // instead use the offset of the first file header
            let first_file_offset = input_exe_zip.by_index(0).unwrap().header_start();
            first_file_offset.try_into().unwrap()
        };
        let mut input_exe: Vec<u8> = vec![0; input_exe_size];
        let mut input_exe_file = std::fs::File::open(input_exe_name).unwrap();
        input_exe_file.read_exact(input_exe.as_mut_slice()).unwrap();
        let perms = input_exe_file.metadata().unwrap().permissions();
        (input_exe, perms)
    };

    // create the output executable
    let mut file = std::fs::File::create("wasm.com").unwrap();
    //file.set_permissions(input_perms).unwrap(); // not wasi compatible
    file.write(input_exe.as_slice()).unwrap();

    // append the zipped files
    let mut zip = zip::ZipWriter::new(file);
    zip.start_file("helloworld.txt", zip::write::FileOptions::default())
        .unwrap();
    zip.write(b"Hello, World!").unwrap();
    zip.finish().unwrap();
}

fn get_hermitfile_path() -> String {
    let mut hermitfile_path = "Hermitfile".to_string();

    let mut use_next = false;
    let args = Box::new(std::env::args());
    for arg in args {
        if use_next {
            hermitfile_path = arg.to_string();
            continue;
        }

        if arg == "-f" {
            use_next = true;
        }

        if arg.starts_with("-f=") {
            let parts = arg.split("=").collect::<Vec<&str>>();
            if parts.len() != 2 {
                continue;
            }

            hermitfile_path = parts[1].to_string();
        }
    }

    hermitfile_path
}

fn main() {
    let hermitfile_path = get_hermitfile_path();
    parse_hermitfile(&hermitfile_path);
}
