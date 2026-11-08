#!/usr/bin/env bash
# Run a command up to ATTEMPTS times, each attempt limited to TIMEOUT (GNU timeout syntax,
# e.g. 5m). A hung attempt is killed and retried instead of leaving the whole job stuck
# until the job timeout cancels it.
#
# Usage: retry.sh ATTEMPTS TIMEOUT -- command [args...]
set -u

if [ $# -lt 4 ] || [ "$3" != "--" ]; then
    echo "usage: $0 ATTEMPTS TIMEOUT -- command [args...]" >&2
    exit 2
fi
attempts=$1
limit=$2
shift 3

for i in $(seq 1 "$attempts"); do
    echo "::group::Attempt $i/$attempts (limit $limit): $*"
    timeout --kill-after=30s "$limit" "$@"
    rc=$?
    echo "::endgroup::"
    if [ $rc -eq 0 ]; then
        exit 0
    fi
    if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
        echo "::warning::Attempt $i/$attempts timed out after $limit: $*"
    else
        echo "::warning::Attempt $i/$attempts failed (exit $rc): $*"
    fi
    if [ "$i" -lt "$attempts" ]; then
        sleep $((i * 10))
    fi
done

echo "::error::Failed after $attempts attempts: $*"
exit 1
