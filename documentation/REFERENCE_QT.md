# Qt Reference Checkout

This page defines a read-only Qt/C++ reference checkout for UI/UX parity work while porting the desktop app to React/Tauri.

The main branch no longer contains the former Qt `src/` tree. Use the reference only to compare screens, labels, layout, behavior, and workflows. New implementation stays in the current React/Tauri tree.

## Canonical path (your machine)

Authoritative Qt/C++ sources should live in a read-only checkout **outside** the inner git repository, for example:

`YOUR_REPO_ROOT\cremniy-main`

(Use the same drive and parent folder as your inner `cremniy` checkout; adjust the placeholder to your actual path.)

That tree includes `src/CMakeLists.txt` with `find_package(Qt6 6.8.3 …)` and the full former `src/` layout. Prefer a **sibling** of the inner repo folder (e.g. `…\Development\cremniy\cremniy` next to `…\Development\cremniy\cremniy-main`), not a mistaken top-level `YOUR_DRIVE:\cremniy-main` unless that is where you intentionally cloned it.

This working copy has **no `.git` directory** — you cannot read a commit SHA from it locally. For a reproducible snapshot, clone using the tag below.

## Recommended layout (fresh clone)

Keep the reference checkout outside the inner repo root, as a sibling directory.

Example:

- Inner repo root: `...\Development\cremniy\cremniy`
- Qt reference: `...\Development\cremniy\cremniy-qt-reference`
- Relative path from the inner repo: `..\cremniy-qt-reference`

On this machine the historical folder name is `cremniy-main` instead of `cremniy-qt-reference`; prefer the clone name below for new checkouts to match the plan.

Adjust the parent path for your machine, but keep the reference outside the inner git repository.

## Clone the reference (tag `pre-qt-removal-2026-05-01`)

From the **parent directory** that will contain the new clone (sibling to your inner `cremniy` folder), run:

```powershell
Set-Location YOUR_REPO_ROOT
git clone https://github.com/igmunv/cremniy cremniy-qt-reference
Set-Location cremniy-qt-reference
git checkout pre-qt-removal-2026-05-01
```

Replace `YOUR_REPO_ROOT` with that parent path (for example the folder that already contains `cremniy\`).

Treat this checkout as local workflow state only. The tag names the snapshot to use for parity work.

## Do Not Commit It

Do not commit the reference tree or copy Qt sources back into the main repo. If your parent workspace can accidentally pick up the folder, you may add `cremniy-qt-reference/` to a local parent-level `.gitignore`.

For repository layout context, see [BMAP](architecture/BMAP.md).
