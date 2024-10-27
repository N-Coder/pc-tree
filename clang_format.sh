#!/bin/bash
#
# Output or fix all files with clang-format that do not conform to the code
# style specified by .clang-format in the root directory.
#
# Author: Max Ilsen

readonly CLANG_VERSION=15
readonly DOCKER_IMAGE="ogdf/clang:$CLANG_VERSION"
readonly CLANG_FORMAT_COMMAND="clang-format-$CLANG_VERSION"
readonly GIT_CLANG_FORMAT_COMMAND="git-clang-format-$CLANG_VERSION"

usage () {
    echo
    echo "$0 [-f] [<git rev>]"
    echo
    echo "Lists files not following code style using $CLANG_FORMAT_COMMAND."
    echo "If $CLANG_FORMAT_COMMAND is not found, a docker image is used instead."
    echo
    echo "If -n is given, docker is always run without sudo."
    echo "If -f is given, the files not adhering to the style are fixed."
    echo "If <git rev> is given, only check/fix lines changed since <git rev>."
}

original_args="$@"
fixit=no
while getopts "fn" opt; do
  case $opt in
    f) fixit=yes ;;
    n) nosudo=yes ;;
    *) usage && exit 255 ;;
  esac
done
shift $(($OPTIND-1))

readonly GIT_REV="$@"
if [ -n "$GIT_REV" ]; then
  git rev-parse --verify "$GIT_REV" > /dev/null 2>&1 ||
    { echo "Given argument is not a valid git revision" && usage && exit 255; }
fi

# Check whether the correct clang-format is available. If not, try to find and use a container runtime.
if ! command -v "$CLANG_FORMAT_COMMAND" > /dev/null 2>&1; then
  if [ -z "$DOCKER_RUN_CMD" ]; then
    # Use podman if available
    if command -v podman > /dev/null 2>&1; then
      DOCKER_RUN_CMD="podman run"
      # (changing the user ID will lead to unsafe git repo location warnings)

    else
      # If user is not in docker group, use sudo.
      prefix="sudo "
      user="$(whoami)"
      if [ -n "$nosudo" ] || \
        groups "$user" | tr " " "\n" | grep "^docker$" || \
        [[ "$OSTYPE" == "darwin"* ]]; then
        prefix=""
      fi

      DOCKER_RUN_CMD="${prefix}docker run --user $(id -u "$user")"
    fi
  fi

  echo "$CLANG_FORMAT_COMMAND not found. Using container (via \`$DOCKER_RUN_CMD\`) instead."

  # Run this script in a docker container.
  repo_dir="$(pwd)"
  $DOCKER_RUN_CMD --rm -ti -w "$repo_dir" \
    -v "$repo_dir":"$repo_dir":rw,z \
    "$DOCKER_IMAGE" \
    "$0" $original_args
  exit_code=$?
  exit $exit_code
fi

# Collect files to check.
files=""
if [ -z "$GIT_REV" ]; then
  files=$(git ls-files |
    grep -e '\.\(cpp\|inc\|h\|hpp\)$')
else
  files=$(git diff-index --diff-filter=AM --name-only "$GIT_REV" |
    grep -e '\.\(cpp\|inc\|h\|hpp\)$')
fi

if [ -z "$files" ]; then
  echo "No files to check since git-rev $GIT_REV."
  exit 0
fi

fail_with_info () {
    echo
    echo "The files mentioned above do not conform to the code style."
    echo
    echo "You can fix that automatically using $0 -f $GIT_REV"
    exit 1
}

perform_test () {
    found=0
    if [ -z "$GIT_REV" ]; then
      if [ "$fixit" = "yes" ]; then
          $CLANG_FORMAT_COMMAND -i $1
      else
          $CLANG_FORMAT_COMMAND --dry-run --Werror $1
          found=$?
      fi
    else
      CLANG_PATH="$(type -p $CLANG_FORMAT_COMMAND)"
      if [ "$fixit" = "yes" ]; then
          $GIT_CLANG_FORMAT_COMMAND --binary "$CLANG_PATH" $GIT_REV -- $1
      else
          $GIT_CLANG_FORMAT_COMMAND --binary "$CLANG_PATH" --diff $GIT_REV -- $1
          found=$?
      fi
    fi
    return $found
}

perform_test "$files" || fail_with_info
