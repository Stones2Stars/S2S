# Project Skills

This directory holds **project-exclusive Claude Code skills** for Stones2Stars.
Unlike personal skills (under `~/.claude/skills/`), these are committed to the
repo, so they are shared by everyone who works on S2S.

## How skills work

Each skill is a directory containing a `SKILL.md` file with YAML frontmatter:

```
.claude/skills/
  <skill-name>/
    SKILL.md          # required: frontmatter + instructions
    <supporting files> # optional: scripts, templates, reference docs
```

`SKILL.md` frontmatter requires two fields:

```markdown
---
name: my-skill
description: One line describing WHAT it does and WHEN to use it. This is the
  only text the model sees when deciding whether to invoke the skill, so be
  specific about the trigger ("Use when the user wants to …").
---

# My Skill

Step-by-step instructions, commands, and context the agent needs to do the task.
```

## Conventions for S2S skills

- `name` must be kebab-case and match the directory name.
- Write the `description` around the trigger — what request should pull this skill in.
- Encode the project's hard-won knowledge (exact build commands, validation steps,
  file wiring rules) so the agent doesn't have to rediscover it. The root
  `AGENTS.md` is the prose reference; skills are the executable procedures.
- Keep DLL constraints in mind (C++2003 only, legacy build chain) when a skill
  generates or edits `Sources/` code.

## Invoking

Users invoke a skill by typing `/<skill-name>`, or the agent invokes it
automatically when a request matches the `description`.

See `build-dll/SKILL.md` for a working example.
