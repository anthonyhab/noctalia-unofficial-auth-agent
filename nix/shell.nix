{
  pkgs ? import <nixpkgs> {},
  bbAuth ? pkgs.callPackage ./default.nix {},
  ...
}: pkgs.mkShell {
  inputsFrom = [ bbAuth ];
  nativeBuildInputs = [ pkgs.clang-tools ];

  shellHook = ''
    # Generate compile_commands.json
    CMAKE_EXPORT_COMPILE_COMMANDS=1 cmake -S . -B ./build
    ln -s build/compile_commands.json .
  '';
}
