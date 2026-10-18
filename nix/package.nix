{ lib, stdenv, ncurses, makeWrapper, python3, docker-client }:
stdenv.mkDerivation {
  pname = "zsh-lab";
  version = lib.trim (builtins.readFile ../VERSION);
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [ ../src ../scripts ../container ../Makefile ../VERSION ../README.md ../LICENSE ];
  };
  nativeBuildInputs = [ makeWrapper python3 ];
  buildInputs = [ ncurses ];
  outputs = [ "out" "tests" ];
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    make host-check tests/smoke-ui
    runHook postCheck
  '';
  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin" "$out/share/doc/zsh-lab" "$tests/bin"
    install -m755 zsh-lab "$out/bin/zsh-lab"
    install -m644 README.md LICENSE "$out/share/doc/zsh-lab/"
    wrapProgram "$out/bin/zsh-lab" --prefix PATH : ${lib.makeBinPath [ docker-client ]}
    install -m755 tests/smoke-ui "$tests/bin/smoke-ui"
    runHook postInstall
  '';
  doInstallCheck = true;
  installCheckPhase = ''
    test "$("$out/bin/zsh-lab" --version)" = "zsh-lab $version"
  '';
  meta = {
    description = "Step-by-step zsh workshop in a disposable Docker environment";
    homepage = "https://github.com/shiroinock/zsh-lab";
    license = lib.licenses.mit;
    platforms = [ "aarch64-darwin" "x86_64-linux" ];
    mainProgram = "zsh-lab";
  };
}
