from __future__ import annotations

import argparse
from pathlib import Path

from . import save_patch, save_project
from .plugins import load_patch_plugin
from .templates import darkwave_ebm_starter


def main() -> int:
    parser = argparse.ArgumentParser(prog="python -m arachnotracker")
    subcommands = parser.add_subparsers(dest="command", required=True)

    starter = subcommands.add_parser("new-ebm", help="write a darkwave/EBM starter project")
    starter.add_argument("output", type=Path)
    starter.add_argument("--title", default="Python EBM Starter")
    starter.add_argument("--bpm", type=float, default=132.0)

    patch_plugin = subcommands.add_parser("patch-plugin", help="run a Python patch plugin")
    patch_plugin.add_argument("plugin", type=Path)
    patch_plugin.add_argument("output", type=Path)
    patch_plugin.add_argument("--factory", default="create_patch")
    patch_plugin.add_argument("--name", default=None)

    args = parser.parse_args()
    if args.command == "new-ebm":
        save_project(darkwave_ebm_starter(args.title, bpm=args.bpm), args.output)
        print(f"Wrote {args.output}")
        return 0

    if args.command == "patch-plugin":
        kwargs = {}
        if args.name is not None:
            kwargs["name"] = args.name
        patch = load_patch_plugin(args.plugin, args.factory, **kwargs)
        save_patch(patch, args.output)
        print(f"Wrote {args.output}")
        return 0

    parser.error("unknown command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
