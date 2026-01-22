derivation {
  name = "failing-build";
  system = builtins.currentSystem;
  builder = "/bin/sh";
  args = [ "-c" "echo 'about to fail'; exit 1" ];
}
