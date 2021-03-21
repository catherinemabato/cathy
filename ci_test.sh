#!/bin/sh

set -e

# Get any test binaries we need
TBINS="test-binaries"
THOST="http://www.spheresystems.co.uk/test-binaries"

if [ "${TRAVIS}" = "true" ] ; then
  export CI="true"
  if [ "${TRAVIS_EVENT_TYPE}" = "cron" ] ; then
    export CI_EVENT="cron"
  fi
  export CI_BRANCH="${TRAVIS_BRANCH}"

elif [ "${GITHUB_ACTIONS}" = "true" ] ; then
  # CI is already set
  if [ "${GITHUB_EVENT_NAME}" = "scheduled" ] ; then
    export CI_EVENT="cron"
  fi
  export CI_BRANCH="$(echo ${GITHUB_REF} | cut -d/ -f3)"
fi

if [ "${CI}" = "true" ] ; then
  [ -d "${HOME}"/cache ] || mkdir "${HOME}"/cache
  [ -h "${TBINS}" ] || ln -s "${HOME}"/cache "${TBINS}"
else
  [ -d "${TBINS}"] || mkdir "${TBINS}"
fi

(
  cd "${TBINS}" || exit 1
  [ -f DR-DOS-7.01.tar ] || wget ${THOST}/DR-DOS-7.01.tar
  [ -f FR-DOS-1.20.tar ] || wget ${THOST}/FR-DOS-1.20.tar
  [ -f MS-DOS-6.22.tar ] || wget ${THOST}/MS-DOS-6.22.tar

  [ -f VARIOUS.tar ] || wget ${THOST}/VARIOUS.tar
)

if [ "${CI_EVENT}" = "cron" ] ; then
  export SKIP_CLASS_THRESHOLD="99"
else
  if [ "${CI_BRANCH}" = "devel" ] ; then
    export SKIP_CLASS_THRESHOLD="2"
  else
    export SKIP_CLASS_THRESHOLD="1"
  fi
fi

# Set FDPP_KERNEL_DIR to non-standard location beforehand
echo
echo "====================================================="
echo "=        Tests run on various flavours of DOS       ="
echo "====================================================="
python3 test/test_dos.py
# single DOS example
# python3 test/test_dos.py FRDOS120TestCase
# single test example
# python3 test/test_dos.py FRDOS120TestCase.test_mfs_fcb_rename_wild_1

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1
