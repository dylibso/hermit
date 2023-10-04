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
    pub map: Vec<[String; 2]>,
    #[serde(rename = "NET")]
    pub net: Vec<String>,
    #[serde(rename = "ENTRYPOINT")]
    pub entrypoint: Vec<String>,
    #[serde(rename = "ENV_PWD_IS_HOST_CWD")]
    pub uses_host_cwd: bool,
    #[serde(rename = "ENV_EXE_NAME_IS_HOST_EXE_NAME")]
    pub uses_host_exe_name: bool,
}

// shoud have a library function exported to take a file path where the Hermitfile is located. this is then open & read via WASI calls, to be parsed into the Hermitfile struct and returned as JSON to the caller

fn main() {
    let dockerfile = Dockerfile::parse(
        r#"
            # locate module w/ entrypoint and dependecies to link exports
FROM mod.wasm
LINK ['dep1.wasm', 'dep2.wasm', 'https://wasmstore:6384/api/v1/module/sqlite3.wasm:{SHA256}'] 

# configure the module instance with environment / resource access 

MAP ['~/.wasm/etc:/etc', 'stdin:stdin', 'stdout:stdout']

NET ['*.github.com', 'api.reddit.com', 'localhost:6379']

# ENV USER=
# ENV NAME=

ENV_PWD_IS_HOST_CWD
        
ENV_EXE_NAME_IS_HOST_EXE_NAME
        
# configure which function to call on startup
ENTRYPOINT ["count_vowels", "this is a test" ]

        "#,
    )
    .unwrap();

    let mut hermitfile = Hermitfile::default();

    for instruction in dockerfile.instructions {
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
                    let mut mappings = vec![];
                    arr.elements.iter().for_each(|s| {
                        let cont = s.content.clone();
                        if cont.contains(":") {
                            let parts = cont.split(":").collect::<Vec<&str>>();
                            mappings.push([parts[0].to_string(), parts[1].to_string()]);
                        }
                    });

                    hermitfile.map = mappings;
                }
            }
            Instruction::Link(ins) => {
                if let Some(arr) = ins.expr.as_exec() {
                    hermitfile.link = arr.elements.iter().cloned().map(|s| s.content).collect();
                }
            }
            Instruction::Entrypoint(ins) => {
                if let Some(arr) = ins.expr.as_exec() {
                    hermitfile.entrypoint =
                        arr.elements.iter().cloned().map(|s| s.content).collect();
                }
            }
            Instruction::EnvPwdIsHostCwd(_) => hermitfile.uses_host_cwd = true,
            Instruction::EnvExeIsHostCwd(_) => hermitfile.uses_host_exe_name = true,
            _ => {}
        }
    }

    println!(
        "{}",
        serde_json::to_string_pretty(&hermitfile).expect("json serialized")
    )
}
