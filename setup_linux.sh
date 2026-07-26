#!/bin/sh
# Setup for building and running tile_engine_retro on Linux Mint / Ubuntu
# and friends. Checks what is already installed, shows what it intends to
# do, and asks before installing anything. After it succeeds:
#
#     make run
#
set -u

echo "=== tile_engine_retro Linux setup ==="
echo
echo "This game is a libretro core. To build and run it you need:"
echo "  1. gcc + make        (compile the core and the asset tool)"
echo "  2. pkg-config        (locate libraries at build time)"
echo "  3. libxmp-dev        (module music playback -- optional but nice)"
echo "  4. retroarch         (the frontend that runs the core)"
echo
echo "Checking what is already here..."
echo

MISSING_PKGS=""
need() {
    # need <label> <package> <check-command...>
    label=$1; pkg=$2; shift 2
    if "$@" >/dev/null 2>&1; then
        echo "  [ok]      $label"
    else
        echo "  [missing] $label  (package: $pkg)"
        MISSING_PKGS="$MISSING_PKGS $pkg"
    fi
}

need "gcc"        "build-essential" sh -c 'command -v gcc'
need "make"       "build-essential" sh -c 'command -v make'
need "pkg-config" "pkg-config"      sh -c 'command -v pkg-config'
need "libxmp"     "libxmp-dev"      pkg-config --exists libxmp
need "retroarch"  "retroarch"       sh -c 'command -v retroarch'

# build-essential may appear twice; deduplicate
MISSING_PKGS=$(echo "$MISSING_PKGS" | tr ' ' '\n' | sort -u | tr '\n' ' ')

echo
if [ -z "${MISSING_PKGS# }" ]; then
    echo "Everything is already installed."
    echo "Build and play with:  make run"
    exit 0
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This system has no apt-get. Install these with your package"
    echo "manager, then run 'make run':"
    echo "   $MISSING_PKGS"
    exit 1
fi

echo "The following command will be run to install what is missing:"
echo
echo "    sudo apt-get install -y $MISSING_PKGS"
echo
echo "(apt-get update only refreshes the package catalog first. Nothing"
echo " already installed on this system gets upgraded or changed -- only"
echo " the packages listed above and their dependencies are added.)"
echo
printf "Proceed? [y/N] "
read -r answer
case "$answer" in
    y|Y|yes|YES)
        # A failing third-party repo (Spotify et al.) must not block us:
        # refresh what refreshes, then install from the main repos.
        sudo apt-get update || \
            echo "(some unrelated repository failed to refresh -- continuing)"
        sudo apt-get install -y $MISSING_PKGS \
            || { echo "Install failed."; exit 1; }
        ;;
    *)
        echo "Nothing installed. Run this script again when ready."
        exit 1
        ;;
esac

echo
echo "Done. Build and play with:  make run"
echo "(In the game: arrows move, A/X fires or drops food, START opens"
echo " the tuner in the Koi River, B returns to the scene menu.)"
