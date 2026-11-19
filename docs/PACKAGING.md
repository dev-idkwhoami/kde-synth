# Arch packaging and GitHub CI

The initial version is `0.1.0`, package name `kde-synth`, display name
**KDE Synthesizer**. Exact-name checks on 2026-11-19 found no package in Arch's
repository search or AUR RPC, and no GitHub repository named `kde-synth`.
These checks are a point-in-time collision check, not a reservation or trademark search.

## Local build

Run `scripts/build-arch-package.sh` as a regular user. Build prerequisites are
listed in the README. The script accepts additional `makepkg` arguments, such as
`--noconfirm`. It never installs the result automatically.

The script stages only the explicit project source list under `work/`, creates a
source archive, calculates SHA-256, and fills `packaging/arch/PKGBUILD.in`.
`makepkg` verifies this checksum before compiling. Private settings, screenshots,
logs, `.git`, build trees and existing binaries are excluded. The generated
PKGBUILD and source archive remain in the stage directory for inspection.

Set `PROJECT_URL=https://github.com/OWNER/REPOSITORY` when building locally to
record the repository URL in package metadata. The default is
`https://github.com/dev-idkwhoami/kde-synth`. CI supplies the actual repository URL automatically.

Outputs: `bin/packages/kde-synth-0.1.0-1-x86_64.pkg.tar.zst` and `SHA256SUMS`.
Checksums detect accidental changes; they are not a publisher signature.

## Workflow

`.github/workflows/arch-package.yml` runs on pushes, pull requests and manual
requests. It updates an official `archlinux:base-devel` container, installs the
build prerequisites, and builds as an unprivileged user. Both C++ and Go checks
and desktop-entry validation must pass before the package is uploaded.

The build job has read-only repository permissions. Checkout and artifact actions
are pinned to commit hashes. It uploads a downloadable artifact with 30-day
retention. A separate release job runs only on tag pushes and only after the build
succeeds. It alone has `contents: write`, downloads that run's artifact, verifies
checksums, and creates a GitHub Release named exactly after the pushed tag, with
the package and checksums attached. Branch pushes, pull requests and manual runs
never create releases, even if the commit already has a tag. This does not
publish to AUR or change a package repository.
The container tracks rolling Arch rather than guaranteeing reproducible builds.
Builds on Arch derivatives can depend on newer system libraries; use the official
Arch CI artifact for distribution to Arch users.

## Release checklist

1. Confirm package metadata points to the intended repository.
2. Choose a version tag such as `v0.1.0` or `0.1.0`.
3. Run the checks and review the package contents with `pacman -Qlp`.
4. Run the GitHub workflow and download its successful artifact.
5. Test installation and playback in a Plasma Wayland session on updated Arch.
6. When ready to publish, create and push the version tag. The tag workflow builds
   and tests that commit again, then publishes its package and checksums.

For example: `git tag v0.1.0` followed by `git push origin v0.1.0`.
The release title and GitHub version use the exact tag. The Arch package version
removes a leading `v` and replaces hyphens with underscores (for example,
`v0.2.0-rc.1` becomes `0.2.0_rc.1`). It must start with a digit and otherwise
contain only letters, digits, dots or underscores; unsupported tags fail before
publication. Untagged builds default to `0.1.0`; local builds can override this
with `PACKAGE_VERSION=0.2.0 ./scripts/build-arch-package.sh`.
A release is not overwritten automatically: rerunning a tag with an existing
release fails the create step, leaving the published release intact.

The installed application uses replaceable shared system Qt/KDE libraries.
The main executable is `/usr/bin/kde-synth`; its private helper is
`/usr/lib/kde-synth/kde-synth-audio`. Uninstalling is managed by pacman.

## Run the workflow locally

With [act](https://nektosact.com/) and a running Docker-compatible engine, run
from a Git checkout of this project:

```sh
mkdir -p work/act-artifacts work/act-cache
act workflow_dispatch \
  -W .github/workflows/arch-package.yml \
  -P ubuntu-latest=archlinux:base-devel \
  --container-daemon-socket - \
  --artifact-server-path "$PWD/work/act-artifacts" \
  --action-cache-path "$PWD/work/act-cache" \
  --no-cache-server \
  --env-file /dev/null --secret-file /dev/null
```

The job itself selects the Arch container. The platform mapping also avoids
act's interactive runner-image prompt. The artifact-server option is required
for local `upload-artifact@v4` validation. No GitHub token is needed for this
public-action workflow, and local `.env` or `.secrets` files are not loaded.
Do not use `--bind`: the job changes ownership inside its workspace, so use act's
normal copy into the container.

For rootless Docker, set `DOCKER_HOST` to its socket. The artifact server must be
reachable from the job's network namespace; a temporary isolated daemon may
require running act in that same namespace. With Docker 29, start that temporary
daemon with `DOCKERD_ROOTLESS_ROOTLESSKIT_DETACH_NETNS=false` so host-network
jobs share the isolated network with act. The host's loopback DNS resolver
must also be reachable or replaced inside that namespace only.

Local act runs exercise package building, tests and artifact upload, but cannot
verify GitHub's hosted-runner provisioning, repository permissions or hosted
artifact storage. Keep the first hosted workflow run as a release check.
