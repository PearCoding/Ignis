#!/usr/bin/env python3
# coding: utf-8

from utils import load_api
import inspect


def parse_obj(obj, ident):
    for name in dir(obj):
        if name.startswith("__"):
            continue

        try:
            data = getattr(obj, name)
        except:
            continue

        if inspect.isclass(data):
            print(f'{ident}{name}: {repr(data)}')
            parse_obj(data, ident=ident+"  ")
        else:
            print(f'{ident}{name}: {repr(data)}')


if __name__ == "__main__":
    ignis = load_api()
    parse_obj(ignis, ident="")
