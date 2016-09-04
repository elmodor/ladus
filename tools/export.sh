#!/bin/sh
blender -b $1 -P "`dirname "$0"`/export.py"
