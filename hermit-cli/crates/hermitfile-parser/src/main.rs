use std::collections::HashMap;
use std::io::SeekFrom;

use dockerfile_parser::{Dockerfile, Instruction};
use serde::Serialize;
use serde_json;
use std::io::Read;
use std::io::Seek;
use std::io::Write;

fn is_false(b: &bool) -> bool {
    *b == false
}

#[derive(Debug, Default, Serialize)]
struct Hermitfile {
    // supported:
    #[serde(rename = "MAP")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub map: Vec<String>,
    #[serde(rename = "ENV")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub env: Vec<String>,
    #[serde(rename = "ENV_PWD_IS_HOST_CWD")]
    #[serde(skip_serializing_if = "is_false")]
    pub uses_host_cwd: bool,
    #[serde(rename = "ENV_EXE_NAME_IS_HOST_EXE_NAME")]
    #[serde(skip_serializing_if = "is_false")]
    pub uses_host_exe_name: bool,
    // not supported yet:
    #[serde(rename = "FROM")]
    #[serde(skip_serializing_if = "String::is_empty")]
    pub from: String,
    #[serde(rename = "LINK")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub link: Vec<String>,
    #[serde(rename = "NET")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub net: Vec<String>,
    #[serde(rename = "ARGV")]
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub argv: Vec<String>,
    #[serde(rename = "ENTRYPOINT")]
    #[serde(skip_serializing_if = "String::is_empty")]
    pub entrypoint: String,
}

fn parse_hermitfile(hermitfile_path: &str) -> Hermitfile {
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

    hermitfile
}

fn create_hermit_executable(hermit: Hermitfile) {
    // load executable to use as the hermit
    let input_exe_name = std::env::var("EXE_NAME").unwrap();
    let (input_exe, input_perms) = {
        let (input_exe_size, mut input_exe_file) = {
            let input_exe_file = std::fs::File::open(&input_exe_name).unwrap();
            let mut input_exe_zip = zip::ZipArchive::new(input_exe_file).unwrap();
            // HACK .offset() doesn't appear to work
            // instead use the offset of the first file header
            let first_file_offset = input_exe_zip.by_index(0).unwrap().header_start();
            let mut exe_file = input_exe_zip.into_inner();
            exe_file.seek(SeekFrom::Start(0)).unwrap();
            (first_file_offset.try_into().unwrap(), exe_file)
        };
        let mut input_exe: Vec<u8> = vec![0; input_exe_size];
        input_exe_file.read_exact(input_exe.as_mut_slice()).unwrap();
        let perms = input_exe_file.metadata().unwrap().permissions();
        (input_exe, perms)
    };

    // create the output executable
    let output_exe_name = "wasm.com";
    let mut file = std::fs::File::create(&output_exe_name).unwrap();
    if file.set_permissions(input_perms).is_err() {
        println!("Unable to make {output_exe_name} executable!");
        println!("Due to platform limitations you must do it yourself! Run:");
        println!("chmod +x {output_exe_name}");
    }
    file.write(input_exe.as_slice()).unwrap();

    // append the zipped files
    let mut zip = zip::ZipWriter::new(file);
    {
        zip.start_file("hermit.json", zip::write::FileOptions::default())
            .unwrap();
        let hermit_json = serde_json::to_string_pretty(&hermit).expect("json serialized");
        zip.write_all(hermit_json.as_bytes()).unwrap();
    }
    {
        zip.start_file("main.wasm", zip::write::FileOptions::default())
            .unwrap();
        let wasm_name = "main.wasm";
        let wasm = std::fs::read(wasm_name).unwrap();
        zip.write_all(&wasm).unwrap();
    }
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
    // hack as wasi doesn't have a wayto set current directory
    if let Ok(input_wd) = std::env::var("PWD") {
        std::env::set_current_dir(input_wd).unwrap();
    }
    let hermitfile_path = get_hermitfile_path();
    let hermit = parse_hermitfile(&hermitfile_path);
    create_hermit_executable(hermit);
}
