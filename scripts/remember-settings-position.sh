#!/usr/bin/env bash
# KWin owns top-level positioning on Wayland. Install one narrowly scoped rule.
set -euo pipefail
rule=kde-synth-settings-position
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}"
if [[ -f "$config_dir/kwinrulesrc" && ! -f "$config_dir/kwinrulesrc.before-kde-synth" ]]; then
    cp -p "$config_dir/kwinrulesrc" "$config_dir/kwinrulesrc.before-kde-synth"
fi
rules=$(kreadconfig6 --file kwinrulesrc --group General --key Order)
if [[ -z "$rules" ]]; then rules=$(kreadconfig6 --file kwinrulesrc --group General --key rules); fi
case ",$rules," in
    *",$rule,"*) ;;
    *) rules="${rules:+$rules,}$rule" ;;
esac
kwriteconfig6 --file kwinrulesrc --group "$rule" --key Description "KDE Synthesizer — remember settings position"
kwriteconfig6 --file kwinrulesrc --group "$rule" --key wmclass kde-synth
kwriteconfig6 --file kwinrulesrc --group "$rule" --key wmclassmatch 1
kwriteconfig6 --file kwinrulesrc --group "$rule" --key title "KDE Synthesizer"
kwriteconfig6 --file kwinrulesrc --group "$rule" --key titlematch 1
kwriteconfig6 --file kwinrulesrc --group "$rule" --key positionrule 4
kwriteconfig6 --file kwinrulesrc --group General --key Order "$rules"
qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure
