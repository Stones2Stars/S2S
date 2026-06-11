# Stones2Stars

A continuation of *Caveman2Cosmos* — a total-conversion mod for
**Sid Meier's Civilization IV: Beyond the Sword**.

## Installation

1. Place this folder inside your Beyond the Sword `Mods` directory so the path is:

   ```
   <Beyond the Sword install>\Mods\Stones2Stars
   ```

   It must live under the **install** directory's `Mods` folder — not under
   *My Documents*.

2. The folder **must** be named `Stones2Stars`. The mod is published to two
   channels with identical content; either can be cloned/checked out straight
   into `Mods` and updated in place later:

   - **git (GitHub):**

     ```
     git clone --depth 1 https://github.com/Stones2Stars/Stones2Stars.git
     ```

     Update later with `git pull`. (`--depth 1` skips old release history and
     makes the initial download much smaller; a plain `git clone` works too.)

   - **SVN (SourceForge):**

     ```
     svn checkout https://svn.code.sf.net/p/stones2stars/code/trunk Stones2Stars
     ```

     Update later with `svn update`.

3. Launch by running `S2S.bat`, or start the game and choose
   *Advanced → Load a Mod → Stones2Stars*.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full release history. It is regenerated
from the project's release tags on every release.
