#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO map viewer GUI (tkinter launcher for map-viewer.py).

Pick a folder with a compiled map (roads.idx, roads.mlp, ... for format V3
or V4, optionally search.idx), choose options, then open a layer in the
viewer window or validate all layers.

    python map-viewer-gui.py [map_dir]

Needs Python with tkinter and the viewer's dependency (pygame). The viewer
itself always runs as a separate process.
"""

import json
import os
import queue
import subprocess
import sys
import tempfile
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

VIEWER = Path(__file__).resolve().with_name("map-viewer.py")
SETTINGS_FILE = Path.home() / ".purrgo-map-viewer-gui.json"

# Order matters: drawing/validation order and the order shown in the list.
LAYERS = ("roads", "landuse", "water", "waterways", "pois")

WHOLE_MAP_KM = 5000.0          # camera that covers any map


# ---------------------------------------------------------------------------
# Map folder scanning (no GUI code, covered by tests)
# ---------------------------------------------------------------------------

@dataclass
class LayerInfo:
    name: str
    idx: Path
    mlp: Optional[Path]
    db: Optional[Path]
    fmt: str            # "V3", "V4" or "?"
    size: int


@dataclass
class MapInfo:
    folder: Path
    layers: List[LayerInfo] = field(default_factory=list)
    search: Optional[Path] = None
    map_name_file: Optional[Path] = None
    title: str = ""
    center: str = ""

    @property
    def formats(self):
        return sorted({layer.fmt for layer in self.layers})


def detect_format(idx_path: Path) -> str:
    """'V3' / 'V4' from the SQT signature at the LOD 0 offset, else '?'."""
    try:
        with open(idx_path, "rb") as f:
            head = f.read(32)
            if len(head) < 32 or head[:3] != b"PGO" or head[3] != 1:
                return "?"
            lod0 = int.from_bytes(head[8:12], "little")
            f.seek(lod0)
            magic = f.read(4)
    except OSError:
        return "?"
    if magic == b"SQT\x01":
        return "V3"
    if magic == b"SQT\x02":
        return "V4"
    return "?"


def scan_map_dir(folder) -> MapInfo:
    folder = Path(folder)
    info = MapInfo(folder=folder)
    for name in LAYERS:
        idx = folder / f"{name}.idx"
        if not idx.is_file():
            continue
        mlp = folder / f"{name}.mlp"
        db = folder / f"{name}.db"
        info.layers.append(LayerInfo(
            name=name,
            idx=idx,
            mlp=mlp if mlp.is_file() else None,
            db=db if db.is_file() else None,
            fmt=detect_format(idx),
            size=sum(p.stat().st_size for p in (idx, mlp, db) if p.is_file()),
        ))
    search = folder / "search.idx"
    if search.is_file():
        info.search = search
    map_name = folder / "map.name"
    if map_name.is_file():
        info.map_name_file = map_name
        try:
            data = json.loads(map_name.read_text(encoding="utf-8"))
            info.title = str(data.get("mapName", ""))
            info.center = f"{float(data['centerLat']):.5f}, {float(data['centerLon']):.5f}"
        except (OSError, ValueError, KeyError, TypeError):
            pass
    return info


def build_view_command(info: MapInfo, layer: LayerInfo, lod: int, size_km: float,
                       screenshot: Optional[str] = None) -> List[str]:
    cmd = [
        sys.executable, str(VIEWER),
        str(layer.idx),
        str(layer.mlp) if layer.mlp else "-",
        str(info.map_name_file) if info.map_name_file else "map.name",
        str(layer.db) if layer.db else "",
        "--size-km", f"{size_km:g}",
    ]
    if layer.fmt == "V4":
        cmd += ["--lod", str(lod)]
    if screenshot:
        cmd += ["--screenshot", screenshot]
    return cmd


def build_search_command(info: MapInfo) -> List[str]:
    cmd = [sys.executable, str(VIEWER), str(info.search)]
    pois_db = info.folder / "pois.db"
    if pois_db.is_file():
        cmd.append(str(pois_db))
    return cmd


def human_size(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024
    return str(n)


def load_settings() -> dict:
    try:
        return json.loads(SETTINGS_FILE.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def save_settings(data: dict) -> None:
    try:
        SETTINGS_FILE.write_text(json.dumps(data, ensure_ascii=False, indent=1),
                                 encoding="utf-8")
    except OSError:
        pass


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
        from tkinter.scrolledtext import ScrolledText
    except ImportError:
        print("tkinter is not installed (Debian/Ubuntu: apt install python3-tk).")
        return 1

    argv = sys.argv[1:] if argv is None else argv
    app = ViewerGui(tk, ttk, filedialog, messagebox, ScrolledText)
    if argv:
        app.set_folder(argv[0])
    app.root.mainloop()
    return 0


class ViewerGui:
    def __init__(self, tk, ttk, filedialog, messagebox, scrolled_text, root=None):
        self.tk, self.ttk = tk, ttk
        self.filedialog, self.messagebox = filedialog, messagebox
        self.root = root or tk.Tk()
        self.root.title("PurrGO map viewer")
        self.root.minsize(760, 560)
        self.info: Optional[MapInfo] = None
        self.busy = False
        self.lines: "queue.Queue" = queue.Queue()
        settings = load_settings()

        self.dir_var = tk.StringVar(value=settings.get("dir", ""))
        self.lod_var = tk.IntVar(value=int(settings.get("lod", 0)))
        self.size_var = tk.StringVar(value=str(settings.get("size_km", "5")))
        self.shot_on = tk.BooleanVar(value=False)
        self.shot_var = tk.StringVar(value=settings.get("screenshot", ""))
        self.whole_var = tk.BooleanVar(value=bool(settings.get("whole_map", True)))
        self.status_var = tk.StringVar(value="Choose a map folder.")
        self.info_var = tk.StringVar(value="")

        self._build(tk, ttk, scrolled_text)
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(50, self._pump)
        if self.dir_var.get() and Path(self.dir_var.get()).is_dir():
            self.set_folder(self.dir_var.get())

    # ----- layout --------------------------------------------------------
    def _build(self, tk, ttk, scrolled_text):
        root = self.root
        pad = {"padx": 8, "pady": 4}
        root.columnconfigure(0, weight=1)
        root.rowconfigure(3, weight=1)

        top = ttk.Frame(root)
        top.grid(row=0, column=0, sticky="ew", **pad)
        top.columnconfigure(1, weight=1)
        ttk.Label(top, text="Map folder:").grid(row=0, column=0, sticky="w")
        entry = ttk.Entry(top, textvariable=self.dir_var)
        entry.grid(row=0, column=1, sticky="ew", padx=6)
        entry.bind("<Return>", lambda _e: self.set_folder(self.dir_var.get()))
        ttk.Button(top, text="Browse…", command=self.browse).grid(row=0, column=2)
        ttk.Button(top, text="Rescan", command=lambda: self.set_folder(self.dir_var.get())
                   ).grid(row=0, column=3, padx=(6, 0))
        ttk.Label(top, textvariable=self.info_var, foreground="#555").grid(
            row=1, column=0, columnspan=4, sticky="w", pady=(4, 0))

        mid = ttk.Frame(root)
        mid.grid(row=1, column=0, sticky="ew", **pad)
        mid.columnconfigure(0, weight=1)

        cols = ("format", "files", "size")
        self.tree = ttk.Treeview(mid, columns=cols, height=6, selectmode="browse")
        self.tree.heading("#0", text="Layer")
        self.tree.heading("format", text="Format")
        self.tree.heading("files", text="Files")
        self.tree.heading("size", text="Size")
        self.tree.column("#0", width=160)
        self.tree.column("format", width=70, anchor="center")
        self.tree.column("files", width=200)
        self.tree.column("size", width=90, anchor="e")
        self.tree.grid(row=0, column=0, sticky="ew")
        self.tree.bind("<Double-1>", lambda _e: self.open_selected())
        self.tree.bind("<<TreeviewSelect>>", lambda _e: self._update_controls())

        opts = ttk.LabelFrame(root, text="Options")
        opts.grid(row=2, column=0, sticky="ew", **pad)
        opts.columnconfigure(5, weight=1)

        ttk.Label(opts, text="LOD (V4):").grid(row=0, column=0, sticky="w", padx=6, pady=4)
        self.lod_buttons = []
        for i in range(4):
            b = ttk.Radiobutton(opts, text=str(i), value=i, variable=self.lod_var)
            b.grid(row=0, column=1 + i, sticky="w", padx=4)
            self.lod_buttons.append(b)

        ttk.Label(opts, text="Camera size, km:").grid(row=1, column=0, sticky="w", padx=6, pady=4)
        ttk.Spinbox(opts, from_=0.1, to=5000, increment=1, width=8,
                    textvariable=self.size_var).grid(row=1, column=1, columnspan=2, sticky="w")
        ttk.Label(opts, text="around the map center (map.name)", foreground="#555"
                  ).grid(row=1, column=3, columnspan=3, sticky="w", padx=6)

        ttk.Checkbutton(opts, text="Save a screenshot instead of opening a window:",
                        variable=self.shot_on).grid(row=2, column=0, columnspan=4,
                                                    sticky="w", padx=6, pady=4)
        ttk.Entry(opts, textvariable=self.shot_var).grid(row=2, column=4, columnspan=2,
                                                          sticky="ew", padx=4)
        ttk.Button(opts, text="…", width=3, command=self.pick_screenshot).grid(
            row=2, column=6, padx=(0, 6))

        ttk.Checkbutton(opts, text="Validate the whole map (camera covers everything; "
                                   "slow on large maps)",
                        variable=self.whole_var).grid(row=3, column=0, columnspan=7,
                                                      sticky="w", padx=6, pady=(0, 4))

        bottom = ttk.Frame(root)
        bottom.grid(row=3, column=0, sticky="nsew", **pad)
        bottom.columnconfigure(0, weight=1)
        bottom.rowconfigure(1, weight=1)
        buttons = ttk.Frame(bottom)
        buttons.grid(row=0, column=0, sticky="ew", pady=(0, 4))
        self.open_btn = ttk.Button(buttons, text="Open selected layer", command=self.open_selected)
        self.open_btn.pack(side="left")
        self.validate_btn = ttk.Button(buttons, text="Validate all layers",
                                       command=self.validate_all)
        self.validate_btn.pack(side="left", padx=6)
        ttk.Button(buttons, text="Clear log", command=self.clear_log).pack(side="right")
        self.log = scrolled_text(bottom, height=12, state="disabled", wrap="none",
                                 font=("TkFixedFont", 9))
        self.log.grid(row=1, column=0, sticky="nsew")
        self.log.tag_configure("ok", foreground="#1a7f37")
        self.log.tag_configure("bad", foreground="#c62828")
        self.log.tag_configure("head", foreground="#1565c0")

        ttk.Label(root, textvariable=self.status_var, relief="sunken", anchor="w"
                  ).grid(row=4, column=0, sticky="ew")

    # ----- folder / state ------------------------------------------------
    def browse(self):
        start = self.dir_var.get() or str(Path.home())
        chosen = self.filedialog.askdirectory(initialdir=start, title="Map folder")
        if chosen:
            self.set_folder(chosen)

    def pick_screenshot(self):
        path = self.filedialog.asksaveasfilename(
            defaultextension=".png", filetypes=[("PNG image", "*.png")])
        if path:
            self.shot_var.set(path)
            self.shot_on.set(True)

    def set_folder(self, folder):
        folder = os.path.abspath(os.path.expanduser(folder)) if folder else ""
        self.dir_var.set(folder)
        self.tree.delete(*self.tree.get_children())
        if not folder or not Path(folder).is_dir():
            self.info = None
            self.info_var.set("")
            self.status_var.set("Folder not found.")
            self._update_controls()
            return
        self.info = scan_map_dir(folder)
        for layer in self.info.layers:
            files = ", ".join(p.suffix for p in (layer.idx, layer.mlp, layer.db) if p)
            self.tree.insert("", "end", iid=layer.name, text=layer.name,
                             values=(layer.fmt, files, human_size(layer.size)))
        if self.info.search:
            self.tree.insert("", "end", iid="search", text="search.idx",
                             values=("V4", "search", human_size(self.info.search.stat().st_size)))
        bits = []
        if self.info.title:
            bits.append(f"map “{self.info.title}”")
        if self.info.center:
            bits.append(f"center {self.info.center}")
        if self.info.formats:
            bits.append("format " + "/".join(self.info.formats))
        self.info_var.set(", ".join(bits))
        kids = self.tree.get_children()
        if kids:
            self.tree.selection_set(kids[0])
            self.status_var.set(f"{len(self.info.layers)} layer(s) found.")
        else:
            self.status_var.set("No map layers (*.idx) in this folder.")
        self._update_controls()

    def _selected(self):
        sel = self.tree.selection()
        return sel[0] if sel else None

    def _update_controls(self):
        ready = self.info is not None and bool(self.tree.get_children()) and not self.busy
        sel = self._selected()
        self.open_btn.state(["!disabled"] if ready and sel else ["disabled"])
        self.validate_btn.state(["!disabled"] if ready else ["disabled"])
        v4 = False
        if self.info and sel and sel != "search":
            v4 = any(layer.name == sel and layer.fmt == "V4" for layer in self.info.layers)
        for b in self.lod_buttons:
            b.state(["!disabled"] if v4 else ["disabled"])

    # ----- running the viewer --------------------------------------------
    def _size_km(self, whole=False) -> Optional[float]:
        if whole:
            return WHOLE_MAP_KM
        try:
            value = float(self.size_var.get().replace(",", "."))
            if value <= 0:
                raise ValueError
            return value
        except ValueError:
            self.messagebox.showerror("Camera size", "Camera size must be a positive number.")
            return None

    def _find_layer(self, name):
        return next((l for l in self.info.layers if l.name == name), None)

    def open_selected(self):
        sel = self._selected()
        if not self.info or not sel or self.busy:
            return
        if sel == "search":
            self._run_sequence([("search.idx", build_search_command(self.info), {})])
            return
        size = self._size_km()
        if size is None:
            return
        layer = self._find_layer(sel)
        shot = None
        if self.shot_on.get():
            shot = self.shot_var.get().strip()
            if not shot:
                self.messagebox.showerror("Screenshot", "Choose a file for the screenshot.")
                return
        cmd = build_view_command(self.info, layer, self.lod_var.get(), size, shot)
        if shot:
            self._run_sequence([(layer.name, cmd, {})])
        else:
            self._spawn_viewer(layer.name, cmd)

    def _spawn_viewer(self, label, cmd):
        """Interactive viewer: independent window, output goes to the log."""
        self._log(f"$ {label}: opening viewer window", "head")
        threading.Thread(target=self._reader, args=(label, cmd, {}, None), daemon=True).start()

    def validate_all(self):
        if not self.info or self.busy:
            return
        size = self._size_km(whole=self.whole_var.get())
        if size is None:
            return
        tmp = tempfile.mkdtemp(prefix="purrgo-view-")
        jobs = []
        for layer in self.info.layers:
            shot = os.path.join(tmp, f"{layer.name}.png")
            cmd = build_view_command(self.info, layer, 0, size, shot)
            jobs.append((layer.name, cmd, {"SDL_VIDEODRIVER": "dummy"}))
        if self.info.search:
            jobs.append(("search.idx", build_search_command(self.info), {}))
        self._run_sequence(jobs, summary=True)

    def _run_sequence(self, jobs, summary=False):
        self.busy = True
        self._update_controls()
        self.status_var.set("Running…")
        threading.Thread(target=self._sequence, args=(jobs, summary), daemon=True).start()

    def _sequence(self, jobs, summary):
        results = []
        for label, cmd, env in jobs:
            self.lines.put((f"=== {label} ===", "head"))
            rc = self._reader(label, cmd, env, True)
            results.append((label, rc))
            self.lines.put((f"{label}: {'OK' if rc == 0 else 'FAILED (exit code %s)' % rc}",
                            "ok" if rc == 0 else "bad"))
        if summary:
            failed = [l for l, rc in results if rc != 0]
            text = ("All layers valid." if not failed
                    else "Failed: " + ", ".join(failed))
            self.lines.put((text, "bad" if failed else "ok"))
        self.lines.put(("__done__", None))

    def _reader(self, label, cmd, extra_env, sequential):
        env = dict(os.environ, PYTHONUNBUFFERED="1", PYTHONIOENCODING="utf-8", **extra_env)
        flags = 0x08000000 if os.name == "nt" and extra_env else 0   # CREATE_NO_WINDOW
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, encoding="utf-8", errors="replace",
                                    env=env, creationflags=flags)
        except OSError as error:
            self.lines.put((f"Cannot start the viewer: {error}", "bad"))
            return -1
        with proc.stdout:
            for line in proc.stdout:
                tag = "bad" if ("[FORMAT ERROR]" in line or "[ERROR]" in line) else None
                self.lines.put((line.rstrip("\n"), tag))
        rc = proc.wait()
        if sequential is None:
            self.lines.put((f"{label}: viewer closed (exit code {rc})",
                            "ok" if rc == 0 else "bad"))
        return rc

    # ----- log -----------------------------------------------------------
    def _log(self, text, tag=None):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n", tag or ())
        self.log.see("end")
        self.log.configure(state="disabled")

    def clear_log(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _pump(self):
        try:
            while True:
                text, tag = self.lines.get_nowait()
                if text == "__done__":
                    self.busy = False
                    self.status_var.set("Done.")
                    self._update_controls()
                else:
                    self._log(text, tag)
        except queue.Empty:
            pass
        self.root.after(50, self._pump)

    def close(self):
        save_settings({
            "dir": self.dir_var.get(),
            "lod": self.lod_var.get(),
            "size_km": self.size_var.get(),
            "screenshot": self.shot_var.get(),
            "whole_map": self.whole_var.get(),
        })
        self.root.destroy()


if __name__ == "__main__":
    sys.exit(main())
