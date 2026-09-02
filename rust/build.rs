use std::env;
use std::path::PathBuf;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let output_file = PathBuf::from(&crate_dir)
        .parent()
        .unwrap()
        .join("include")
        .join("notifly_c_rust.h");

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(cbindgen::Config::from_file(
            PathBuf::from(&crate_dir).join("cbindgen.toml"),
        ).unwrap())
        .generate()
        .expect("Unable to generate C bindings")
        .write_to_file(output_file);
}
