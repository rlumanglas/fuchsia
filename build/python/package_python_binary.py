#!/usr/bin/env python3.8
"""Creats a Python zip archive for the input main source."""

# Copyright 2021 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import shutil
import sys
import zipapp


def main():
    parser = argparse.ArgumentParser(
        'Creates a Python zip archive for the input main source')

    parser.add_argument(
        '--main_source',
        help='Path to the source containing the main function',
        required=True,
    )
    parser.add_argument(
        '--main_callable',
        help=
        'Name of the the main callable, that is the entry point of the generated archive',
        required=True,
    )

    parser.add_argument(
        '--out_dir',
        help='Path to output directory',
        required=True,
    )
    parser.add_argument('--output', help='Path to output', required=True)

    parser.add_argument(
        '--sources',
        help='Sources of this target, including main source',
        nargs='*',
    )
    parser.add_argument(
        '--library_infos',
        help='Path to the library infos JSON file',
        type=argparse.FileType('r'),
        required=True,
    )
    parser.add_argument(
        '--depfile',
        help='Path to the depfile to generate',
        type=argparse.FileType('w'),
        required=True,
    )

    args = parser.parse_args()

    infos = json.load(args.library_infos)
    # For writing a depfile.
    files_to_copy = []
    # For cleaning up the temporary app directory after zipapp.
    files_to_delete, dirs_to_delete = [], []

    # Temporary directory to stage the source tree for this python binary,
    # including sources of itself and all the libraries it imports.
    app_dir = os.path.join(args.out_dir, 'app')
    os.makedirs(app_dir, exist_ok=True)

    # Copy over the sources of this binary.
    for source in args.sources:
        dest = os.path.join(app_dir, os.path.basename(source))
        shutil.copy2(source, dest)
        files_to_delete.append(dest)

    # Make sub directories for all libraries and copy over their sources.
    for info in infos:
        dest_dir = os.path.join(app_dir, info['library_name'])
        os.makedirs(dest_dir, exist_ok=True)
        dirs_to_delete.append(dest_dir)

        for source in info['sources']:
            files_to_copy.append(source)
            dest = os.path.join(dest_dir, os.path.basename(source))
            shutil.copy2(source, dest)
            files_to_delete.append(dest)

    args.depfile.write('{}: {}\n'.format(args.output, ' '.join(files_to_copy)))

    # Main module is the main source without its extension.
    main_module = os.path.splitext(os.path.basename(args.main_source))[0]
    zipapp.create_archive(
        app_dir,
        target=args.output,
        main=f'{main_module}:{args.main_callable}',
        interpreter='/usr/bin/env python3.8',
        compressed=True,
    )

    # Manually remove the temporary app directory and all the files, instead of
    # using shutil.rmtree. rmtree records reads on directories which throws off
    # the action tracer.
    for f in files_to_delete:
        os.remove(f)
    for d in dirs_to_delete:
        os.rmdir(d)


if __name__ == '__main__':
    sys.exit(main())