{ lib, stdenv, ncurses, zsh, vim, makeWrapper }:
stdenv.mkDerivation {
  pname = "zsh-lab";
  version = lib.trim (builtins.readFile ../VERSION);

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../src
      ../Makefile
      ../VERSION
      ../README.md
      ../LICENSE
    ];
  };

  env.LANG = "en_US.UTF-8";
  env.LC_ALL = "en_US.UTF-8";

  nativeBuildInputs = [ makeWrapper ];
  buildInputs = [ ncurses ];
  nativeCheckInputs = [ zsh vim ];

  doCheck = true;
  checkPhase = ''
    runHook preCheck
    export ZSHLAB_TEST_EDITOR="${vim}/bin/vi -Nu NONE -i NONE"
    make test
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin" "$out/share/doc/zsh-lab"
    install -m755 zsh-lab "$out/bin/zsh-lab"
    install -m644 README.md LICENSE "$out/share/doc/zsh-lab/"
    wrapProgram "$out/bin/zsh-lab" --prefix PATH : ${lib.makeBinPath [ zsh ]}
    runHook postInstall
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    test "$("$out/bin/zsh-lab" --version)" = "zsh-lab $version"
    make tests/smoke-ui
    ./tests/smoke-ui "$out/bin/zsh-lab"
    runHook postInstallCheck
  '';

  meta = {
    description = "Step-by-step terminal practice for zsh, ZLE widgets and key bindings";
    homepage = "https://github.com/shiroinock/zsh-lab";
    license = lib.licenses.mit;
    platforms = [ "aarch64-darwin" ];
    mainProgram = "zsh-lab";
  };
}
