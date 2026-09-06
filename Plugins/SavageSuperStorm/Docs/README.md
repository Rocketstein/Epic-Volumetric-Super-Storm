# Savage Super Storm — documentation

The plugin's user documentation. This folder holds the **sources**; the readable
site is produced locally with [MkDocs](https://www.mkdocs.org/) and the
[Material](https://squidfunk.github.io/mkdocs-material/) theme.

## Viewing the docs

You need Python 3.9 or newer. Everything below is one-time setup except the last
step.

**1. Get the files**

```
p4 sync //depot/VolumetricSuperStorm/Plugins/SavageSuperStorm/Docs/...
```

**2. Create an environment and install the toolchain**

Put the virtualenv *outside* the Perforce workspace so its files never show up
in `p4 reconcile`:

```
python -m venv %LOCALAPPDATA%\VSSDocsEnv
%LOCALAPPDATA%\VSSDocsEnv\Scripts\python.exe -m pip install -r requirements.txt
```

**3. Serve the site**

From this folder:

```
%LOCALAPPDATA%\VSSDocsEnv\Scripts\mkdocs.exe serve
```

Then open <http://127.0.0.1:8000/VolumetricSuperstorm_Docs/>. The page reloads
automatically whenever a source file is saved.

Versions are pinned in `requirements.txt`, so everyone builds the same site.
MkDocs is deliberately held below 2.0, which drops the plugin system Material
depends on.

## Editing

Pages are Markdown under `docs/`, and the navigation is the `nav:` block in
`mkdocs.yml`. A page must be listed there to appear in the sidebar.

To check your work before submitting:

```
%LOCALAPPDATA%\VSSDocsEnv\Scripts\mkdocs.exe build --clean
```

`strict: true` is enabled, so the build **fails** on a broken internal link or a
`nav` entry pointing at a missing file. Exit code 0 means the site is sound.

Two things that build will *not* catch:

- A page deleted from disk but never referenced by another page disappears
  silently — it only reports an `INFO` line.
- Paths inside raw HTML (`<video src=...>`) are not validated. Markdown image
  links are, so prefer `![](...)` over `<img>` wherever possible.

### Media

Screenshots go in `docs/assets/images/`, video in `docs/assets/videos/`.

Markdown links and raw HTML resolve paths differently, which is worth knowing
when a file will not load:

| Form | Example path from a page in `docs/guides/` |
|---|---|
| Markdown `![](...)` — resolved by MkDocs, relative to the source file | `../assets/images/foo.png` |
| Raw HTML `src` — resolved by the browser, relative to the built URL | `../../assets/videos/foo.mp4` |

For two videos side by side, wrap the figures in `<div class="grid" markdown>`;
Material handles the responsive layout with no extra CSS.

## What not to submit

`site/` is generated output and is excluded in `.p4ignore` along with `.venv`.
Submit only the sources: `docs/`, `mkdocs.yml`, `requirements.txt`, and this
file.
