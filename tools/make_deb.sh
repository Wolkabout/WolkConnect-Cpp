#!/usr/bin/env bash

# Check what the current branch is
if [ $# -eq 1 ]; then
  branch=$1
else
  branch=$(git rev-parse --abbrev-ref HEAD)

  if [ "$branch" == "" ]; then
    echo "You must specify a branch as parameter to the script (if the script is not part of a repo)"
    exit
  fi
fi

echo "$branch"

# Get the git repository cloned
rm -rf tmp
mkdir -p tmp
cd tmp || exit
git clone --recurse-submodules https://github.com/Wolkabout/WolkConnect-Cpp

# Enter the repo, checkout to branch, update submodules
cd WolkConnect-Cpp || exit
git checkout "$branch"
git submodule update --recursive

# Run the build
debuild -us -uc -b -j"$(nproc)"
