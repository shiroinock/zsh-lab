{
  description = "ZSH LAB — an interactive zsh workshop in your terminal";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-26.05-darwin";

  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-darwin" "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          zsh-lab = pkgs.callPackage ./nix/package.nix { };
        in {
          inherit zsh-lab;
          default = zsh-lab;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/zsh-lab";
          meta.description = "Practice zsh key bindings and ZLE widgets";
        };
      });

      checks = forAllSystems (system: {
        zsh-lab = self.packages.${system}.default;
      });
    };
}
