{ lib, stdenv, ncurses, zsh, vim, makeWrapper, glibcLocalesUtf8, bubblewrap }:
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
  nativeCheckInputs = [ zsh vim ] ++ lib.optionals stdenv.isLinux [ glibcLocalesUtf8 bubblewrap ];

  # Nix's builder sandbox forbids nesting sandbox-exec / user namespaces.
  # Compile here; CI executes these exact installed tests outside the builder.
  outputs = [ "out" "tests" ];
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    make tests/test-native tests/smoke-ui
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin" "$out/share/doc/zsh-lab"
    install -m755 zsh-lab "$out/bin/zsh-lab"
    install -m644 README.md LICENSE "$out/share/doc/zsh-lab/"
    wrapProgram "$out/bin/zsh-lab" --prefix PATH : ${lib.makeBinPath ([ zsh ] ++ lib.optional stdenv.isLinux bubblewrap)} \
      ${lib.optionalString stdenv.isLinux "--set-default LOCALE_ARCHIVE ${glibcLocalesUtf8}/lib/locale/locale-archive"}
    mkdir -p "$tests/libexec" "$tests/bin"
    install -m755 tests/test-native tests/smoke-ui "$tests/libexec/"
    makeWrapper "$tests/libexec/test-native" "$tests/bin/test-native" \
      --prefix PATH : ${lib.makeBinPath ([ zsh vim ] ++ lib.optional stdenv.isLinux bubblewrap)} \
      --set ZSHLAB_TEST_EDITOR "${vim}/bin/vi -Nu NONE -i NONE" \
      ${lib.optionalString stdenv.isLinux "--set-default LOCALE_ARCHIVE ${glibcLocalesUtf8}/lib/locale/locale-archive"}
    makeWrapper "$tests/libexec/smoke-ui" "$tests/bin/smoke-ui" \
      --prefix PATH : ${lib.makeBinPath ([ zsh ] ++ lib.optional stdenv.isLinux bubblewrap)}
    runHook postInstall
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    test "$("$out/bin/zsh-lab" --version)" = "zsh-lab $version"
    runHook postInstallCheck
  '';

  meta = {
    description = "Step-by-step terminal practice for zsh, ZLE widgets and key bindings";
    homepage = "https://github.com/shiroinock/zsh-lab";
    license = lib.licenses.mit;
    platforms = [ "aarch64-darwin" "x86_64-linux" ];
    mainProgram = "zsh-lab";
  };
}
