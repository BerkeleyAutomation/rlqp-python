#!/bin/bash

if [ -z "$VIRTUAL_ENV" ] ; then
    echo "Refusing to build, virtualenv not set"
    exit 1
fi

cd "$(dirname "$0")"

pip uninstall -y rlqp
rm -rf build dist rlqp.egg-info .eggs rlqp_sources/build extension/src/rlqp.a
python setup.py install
python setup.py test
