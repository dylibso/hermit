use dockerfile_parser::{Dockerfile, Instruction};

#[derive(Debug, Default)]
struct Hermitfile {
    pub from: String,
    pub link: Vec<String>,
    pub map: Vec<[String; 2]>,
    pub net: Vec<String>,
    pub entrypoint: Vec<String>,
}

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
            _ => {}
        }
    }

    println!("{:#?}", hermitfile);
}
