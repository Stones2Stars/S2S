# Stones2Stars

A continuation of *Caveman2Cosmos* — a total-conversion mod for
**Sid Meier's Civilization IV: Beyond the Sword**.

This is the **development repository** (C++ DLL sources, XML data, Python
callbacks, tooling). You don't need anything from here to *play* the mod —
grab a release from one of the distribution channels below.

## Getting a release

Releases are published to two channels with identical content; use whichever
client you prefer. Either way, the mod must end up in your Beyond the Sword
`Mods` folder, in a folder named exactly `Stones2Stars`:

```
<Beyond the Sword install>\Mods\Stones2Stars
```

(Under the game's **install** directory — not under *My Documents*.)

### Option A — git (GitHub)

```bat
cd /d "<Beyond the Sword install>\Mods"
git clone --depth 1 https://github.com/Stones2Stars/Stones2Stars.git
```

Update to the latest release later by running `git pull` inside the
`Stones2Stars` folder. (`--depth 1` skips old release history and makes the
initial download much smaller; a plain `git clone` works too.)

### Option B — SVN (SourceForge)

```bat
cd /d "<Beyond the Sword install>\Mods"
svn checkout https://svn.code.sf.net/p/stones2stars/code/trunk Stones2Stars
```

Update to the latest release later by running `svn update` inside the
`Stones2Stars` folder.

### Launching

Run `S2S.bat` in the mod folder, or start Beyond the Sword and choose
*Advanced → Load a Mod → Stones2Stars*. Full install notes are in the release
[README](docs/MOD-README.md); each release also ships a `CHANGELOG.md`.

## Development

This repository holds the sources the releases are built from:

- `Sources/` — the C++ game DLL (`CvGameCoreDLL.dll`)
- `Assets/XML/`, `Assets/Python/` — game data and script callbacks
- `Tools/` — build and validation tooling

Start with [AGENTS.md](AGENTS.md) for repository layout, build instructions
(`Tools/_Build.ps1`, FastBuild) and conventions; developer docs live in
[`Sources/docs/`](Sources/docs/README.md). Pushing the `release` branch
triggers the AppVeyor build that publishes to both channels above.

## Community

- Discord: [discord.gg/R8Uejx6uaK](https://discord.gg/R8Uejx6uaK)
- [The Despair Index](docs/DESPAIR_INDEX.md) — a rigorously unscientific
  ranking of bugs unearthed by the rework
