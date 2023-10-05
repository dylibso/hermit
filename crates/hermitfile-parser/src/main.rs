use std::collections::HashMap;

use dockerfile_parser::{Dockerfile, Instruction};
use serde::Serialize;
use serde_json;

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

// shoud have a library function exported to take a file path where the Hermitfile is located. this is then open & read via WASI calls, to be parsed into the Hermitfile struct and returned as JSON to the caller
#[no_mangle]
pub extern "C" fn parse_hermitfile(ptr: *mut u8, len: u32) {
    let hermitfile_path = unsafe { String::from_raw_parts(ptr, len as usize, len as usize) };

    let file = std::fs::read(&hermitfile_path).unwrap_or_default();
    let dockerfile = String::from_utf8(file).unwrap_or_default();

    println!("{}", dockerfile);

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
    )
}

fn main() {
    const hermitfile_path: &str = "Hermitfile";

    parse_hermitfile(
        hermitfile_path.as_ptr() as *mut u8,
        hermitfile_path.len() as u32,
    )
}
