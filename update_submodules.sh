#!/bin/bash
# -----------------------------------------------------------------------------
# update_submodules.sh
# Helper script to initialize and update all submodules (recursively)
# for open62541pp and its dependencies.
# -----------------------------------------------------------------------------

set -e  # Exit on error
set -o pipefail

echo "Updating submodules for open62541pp..."

# Go to the script directory (so it works when run from anywhere)
cd "$(dirname "$0")/.."

# Initialize and update all submodules recursively
git submodule update --init --recursive

echo "Submodules initialized and updated."

# Optional: Show current status
echo
echo "Current submodule status:"
git submodule status

echo
echo "Done."

