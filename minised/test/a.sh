#!/usr/bin/env bash

# ע�⣺ƥ��windows·���е�\��minised����\\�ͺã�������sed�е���\\\\
cat main.inc | sed -e "/Note: including file:[ \t]\+C:\\\\Program Files/d" -e "/Note: including file:/w bigsed.txt" -e "/Note: including file:/d"




