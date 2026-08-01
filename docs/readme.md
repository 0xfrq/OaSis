---
layout: default
title: Documentation guide
description: Navigate, preview, and maintain the OaSis documentation site.
content_type: how-to
audience: contributors
---

# Documentation guide

The OaSis documentation is a Jekyll site. It combines a short project overview with detailed pages about booting, kernel subsystems, hardware drivers, networking, filesystems, applications, testing, and planned GUI work.

## Browse the published site

The published documentation is available at [oasis.fariqdoing.tech](https://oasis.fariqdoing.tech).

## Run the site locally

Install Ruby, Bundler, and Jekyll through your operating system or Ruby environment. Then run:

```bash
cd docs
jekyll serve
```

Open the local URL printed by Jekyll. The site reads navigation from `_config.yml` and renders Markdown pages through `_layouts/default.html`.

## Documentation structure

- `index.md`: landing page and current implementation status.
- `01-pendahuluan/`: project goals and terminology.
- `02-arsitektur/`: kernel layers and data flow.
- `03-booting/`: Multiboot entry and `kernel_main()` initialization.
- `04-kernel/`: memory, tasks, interrupts, and system calls.
- `05-driver/`: hardware driver catalog and networking internals.
- `06-filesystem/`: OAFS structures and operations.
- `07-shell/`: command reference.
- `08-apps/`: editor, assembler, and `occ` compiler.
- `09-build/`: build workflow and QEMU configuration.
- `09-build/testing.md`: host protocol tests and manual integration tests.
- `10-changelog/`: dated fixes and feature history.
- `11-gui/`: future framebuffer and desktop roadmap.

## Writing and maintenance rules

Keep documentation aligned with source code:

- Use sentence-case headings and English terminology.
- Add a language tag to every fenced code block. Use `c`, `asm`, `bash`, or `text` when appropriate; diagrams and terminal output use `text`.
- Keep code examples short and use the language that matches the actual source.
- Use ASCII in kernel output examples because VGA text mode does not render Unicode reliably.
- Mark features as Implemented, Partial, Experimental, or Planned.
- Document QEMU-only assumptions and static configuration values.
- Do not claim that TCP, UDP, DHCP, sockets, network syscalls, or a GUI are implemented.
- Link to detailed pages instead of duplicating long explanations.

After changing navigation or front matter, run Jekyll locally and inspect the landing page, mobile menu, code blocks, tables, and generated table of contents.
