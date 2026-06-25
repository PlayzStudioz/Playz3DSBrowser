# Playz 3DS Browser

A minimal native homebrew web browser for the Nintendo 3DS — pink, yellow, and cyan themed.

- **Top screen**: renders the current page (title bar + word-wrapped text, links shown as `[link text]`).
- **Bottom screen**: Back / Forward / Reload buttons, address bar, scrollable link list.
- **Circle Pad**: moves the cursor. **A**: select. **B**: back. **D-Pad Up/Down**: scroll page. **L/R**: scroll links. **START**: quit.

> **HTTP only — no HTTPS.** Test with `http://example.com/` or other plain-HTTP pages.

---

## How to set up (no CLI required)

### Step 1 — Create a GitHub repo

Go to [github.com/new](https://github.com/new), name it `playz-3ds-browser`, make it **Public**, and click **Create repository**. Leave it completely empty.

---

### Step 2 — Upload the project files

1. On your new repo page, click **"uploading an existing file"**.
2. Unzip the project on your computer. Open the `3ds-browser/` folder.
3. **Show hidden files** first, so you can see the `.github/` folder and `.gitignore`:
   - **Windows**: In File Explorer, check View → Hidden items
   - **Mac**: Press `Cmd + Shift + .` to toggle hidden files in Finder
4. Drag **all** the files and folders into the GitHub upload window.
5. Click **Commit changes**.

---

### Step 3 — Add the build workflow (the important part)

The `.github/workflows/build.yml` file is the one that auto-builds your app. Because it lives in a hidden folder, drag-and-drop often misses it. Add it manually:

1. On your GitHub repo, click **Add file → Create new file**.
2. In the filename box at the top, type exactly:
   ```
   .github/workflows/build.yml
   ```
   (GitHub will auto-create the folders as you type the slashes.)
3. Open `BUILD_WORKFLOW.yml` from your unzipped project in any text editor (Notepad, TextEdit, etc.), select all, and copy it.
4. Paste it into the big text area on GitHub.
5. Click **Commit new file**.

> `BUILD_WORKFLOW.yml` in the zip is just a plain-text copy of the workflow for this step — GitHub needs it at the `.github/workflows/build.yml` path.

---

### Step 4 — Watch it build

Click the **Actions** tab on your repo. You'll see "Build Playz 3DS Browser" running (takes 2–4 minutes).

- ✅ Green = success
- ❌ Red = something needs a small fix. Click in, read the error, and paste it to an LLM for help. The `.3dsx` (Homebrew Launcher) build almost always succeeds; the CIA step is the one that occasionally needs a `template.rsf` tweak.

---

### Step 5 — Get your files and install

**Homebrew Launcher (easiest — no FBI needed)**
1. Actions → click the completed run → **Artifacts** → download `playz-3ds-browser-3dsx`.
2. Unzip and copy `playz-3ds-browser.3dsx` to `/3ds/playz-3ds-browser/` on your SD card.
3. Launch Homebrew Launcher and pick it from the list.

**CIA → FBI install (shows on Home Menu)**
1. On GitHub go to **Releases → Create a new release**, set tag to `v1.0`, publish it. The workflow automatically attaches the `.cia` to it.
2. On the Releases page, right-click the `.cia` → **Copy link address**.
3. Turn that URL into a QR code at [qr-code-generator.com](https://www.qr-code-generator.com/).
4. On your 3DS: **FBI → Remote Install** → scan the QR code.

---

## File layout
```
source/              — all C source (http.c/.h, htmlparse.c/.h, ui.c/.h, main.c)
Makefile             — build script
template.rsf         — CIA packaging spec
icon.png             — 48×48 app icon
banner_image.png     — Home Menu banner
banner_audio.wav     — silent banner audio
BUILD_WORKFLOW.yml   — copy of the GitHub Actions workflow (see Step 3 above)
.github/workflows/   — where GitHub actually reads the workflow from
.gitignore
```
