#!/usr/bin/env bash
set -euo pipefail

if [ ! -d .git ]; then
  git init
fi

git add .
git status

echo
echo "Next:"
echo "  git commit -m \"Initial Pluto+ SDR Windows toolkit\""
echo "  git branch -M main"
echo "  git remote add origin https://github.com/bakstaaj/<REPO-NAME>.git"
echo "  git push -u origin main"
