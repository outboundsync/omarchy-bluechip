#!/usr/bin/env bash
# Bluechip installer for Omarchy.
# - Symlinks bin/bluechip + bin/bluechip-switch into ~/.local/bin
# - Checks for sf + jq
# - Prints (and optionally applies) the Waybar wiring
# Read-only tool; this installer touches only your dotfiles.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DST="${HOME}/.local/bin"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
WAYBAR_DIR="$CONFIG_HOME/waybar"

bold=$(tput bold 2>/dev/null || true); dim=$(tput dim 2>/dev/null || true)
grn=$(tput setaf 2 2>/dev/null || true); ylw=$(tput setaf 3 2>/dev/null || true)
red=$(tput setaf 1 2>/dev/null || true); rst=$(tput sgr0 2>/dev/null || true)
say()  { printf '%s\n' "$*"; }
ok()   { printf '  %s✓%s %s\n' "$grn" "$rst" "$*"; }
warn() { printf '  %s!%s %s\n' "$ylw" "$rst" "$*"; }

say "${bold}Installing Bluechip${rst} ${dim}(Salesforce admin cockpit for Omarchy)${rst}"

# 1) Dependencies -----------------------------------------------------------
command -v jq >/dev/null 2>&1 && ok "jq found" || {
  warn "jq not found — install it:  sudo pacman -S jq"; }
if command -v sf >/dev/null 2>&1; then
  ok "sf found ($(sf --version 2>/dev/null | head -1))"
else
  warn "sf (Salesforce CLI) not found."
  say  "     Install:  ${bold}npm i -g @salesforce/cli${rst}   then   ${bold}sf org login web${rst}"
fi

# 2) Link the executables ---------------------------------------------------
mkdir -p "$BIN_DST"
ln -sf "$REPO_DIR/bin/bluechip"        "$BIN_DST/bluechip"
ln -sf "$REPO_DIR/bin/bluechip-switch" "$BIN_DST/bluechip-switch"
chmod +x "$REPO_DIR/bin/bluechip" "$REPO_DIR/bin/bluechip-switch"
ok "linked bluechip + bluechip-switch → $BIN_DST"
case ":$PATH:" in
  *":$BIN_DST:"*) : ;;
  *) warn "$BIN_DST is not on your PATH — add it to your shell rc." ;;
esac

# 3) Waybar wiring ----------------------------------------------------------
say ""
say "${bold}Waybar${rst}"
if [[ -d "$WAYBAR_DIR" ]]; then
  # CSS
  if [[ -f "$WAYBAR_DIR/style.css" ]] && ! grep -q 'custom-bluechip' "$WAYBAR_DIR/style.css" 2>/dev/null; then
    {
      printf '\n/* --- Bluechip (added by install.sh) --- */\n'
      cat "$REPO_DIR/waybar/bluechip.css"
    } >> "$WAYBAR_DIR/style.css"
    ok "appended chip styles → $WAYBAR_DIR/style.css"
  else
    warn "style.css already has bluechip styles (or no style.css) — skipped"
  fi
  say  "  ${dim}To finish, add the module to a modules-* array and paste the object from${rst}"
  say  "  ${dim}waybar/config.snippet.jsonc into $WAYBAR_DIR/config.jsonc, then:${rst}"
  say  "     ${bold}pkill -SIGUSR2 waybar${rst}   ${dim}# reload${rst}"
else
  warn "no ~/.config/waybar — is this Omarchy? Chip step skipped; CLI still works."
fi

# 4) Smoke test -------------------------------------------------------------
say ""
if command -v sf >/dev/null 2>&1 && sf org display >/dev/null 2>&1; then
  ok "Authed org detected — try:  ${bold}bluechip doctor${rst}"
else
  say "  ${dim}Next:${rst}  sf org login web   &&   ${bold}bluechip doctor${rst}"
fi
say "${grn}${bold}Done.${rst} bluechip is read-only and uses your own sf login."
