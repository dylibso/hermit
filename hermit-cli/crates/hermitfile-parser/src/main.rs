use std::collections::HashMap;
use std::io::SeekFrom;

use clap;
use dockerfile_parser::{Dockerfile, Instruction};
use serde::Serialize;
use std::io::Read;
use std::io::Seek;
use std::io::Write;

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
    #[serde(skip_serializing_if = "std::ops::Not::not")]
    pub uses_host_cwd: bool,
    #[serde(rename = "ENV_EXE_NAME_IS_HOST_EXE_NAME")]
    #[serde(skip_serializing_if = "std::ops::Not::not")]
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

fn parse_hermitfile(hermitfile_path: &std::ffi::OsStr) -> Hermitfile {
    let hf = {
        let dockerfile = {
            let file = match std::fs::read(hermitfile_path) {
                Ok(file) => file,
                _ => panic!("Error reading {:?}", &hermitfile_path),
            };
            match String::from_utf8(file) {
                Ok(dockerfile) => dockerfile,
                _ => panic!("{:?} is not valid UTF-8", &hermitfile_path),
            }
        };
        match Dockerfile::parse(&dockerfile) {
            Ok(hf) => hf,
            _ => panic!("{:?} has invalid Hermitfile syntax", &hermitfile_path),
        }
    };

    let mut hermitfile = Hermitfile::default();
    let mut env_map: HashMap<String, String> = HashMap::new();

    for instruction in hf.instructions {
        match instruction {
            Instruction::From(ins) => {
                let mut path = std::path::PathBuf::from(&hermitfile_path);
                assert!(path.pop());
                path.push(ins.image.content);
                let wasm_path = path.into_os_string().into_string().unwrap();
                hermitfile.from = wasm_path;
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

    if hermitfile.from.is_empty() {
        panic!("Missing mandatory item: FROM");
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

fn create_hermit_executable(output_exe_name: &std::ffi::OsStr, hermit: Hermitfile) {
    // load executable to use as the hermit
    let (input_exe, input_perms) = {
        let (input_exe_size, mut input_exe_file) = {
            let input_exe_name = match std::env::var("EXE_NAME") {
                Ok(input_exe_name) => input_exe_name,
                _ => panic!("$EXE_NAME must be provided to build a hermit executable"),
            };
            let input_exe_file = match std::fs::File::open(&input_exe_name) {
                Ok(input_exe_file) => input_exe_file,
                _ => panic!("Error opening {input_exe_name}"),
            };
            let mut input_exe_zip = match zip::ZipArchive::new(input_exe_file) {
                Ok(input_exe_file) => input_exe_file,
                _ => panic!("Error opening {input_exe_name}, is it a ZIP/APE file?"),
            };
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
    let mut file = std::fs::File::create(output_exe_name).unwrap();
    if file.set_permissions(input_perms).is_err() {
        println!("Unable to make {:?} executable!", output_exe_name);
        println!("Due to platform limitations you must do it yourself! Run:");
        println!("chmod +x {:?}", output_exe_name);
    }
    file.write_all(input_exe.as_slice()).unwrap();

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
        let wasm = match std::fs::read(&hermit.from) {
            Ok(wasm) => wasm,
            _ => panic!("Error opening {}", hermit.from),
        };
        zip.write_all(&wasm).unwrap();
    }
    zip.finish().unwrap();
}

#[derive(clap::Parser)]
#[command(about, author, bin_name = "hermit.com", long_version = None, version)]
struct HermitCliArgs {
    /// Name of the `Hermitfile`
    #[arg(default_value = "Hermitfile", short = 'f')]
    hermitfile_path: std::ffi::OsString,
    /// Output file
    ///
    /// on `unix`-like platforms, you will need to run `chmod +x <OUTPUT_PATH>`
    /// to make it executable. This is required becasue `WASI` does not have a
    /// `chmod` function.
    #[arg(default_value = "main.wasm", short = 'o')]
    output_path: std::ffi::OsString,
}

impl HermitCliArgs {
    #[inline]
    fn parse_args() -> Self {
        clap::Parser::parse()
    }
}

fn main() {
    // hack as wasi doesn't have a wayto set current directory
    if let Ok(input_wd) = std::env::var("PWD") {
        std::env::set_current_dir(input_wd).unwrap();
    }
    let options = HermitCliArgs::parse_args();
    let hermit = parse_hermitfile(&options.hermitfile_path);
    create_hermit_executable(&options.output_path, hermit);
}
