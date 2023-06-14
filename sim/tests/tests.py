#!/usr/bin/env python3

import json
import subprocess
import os
import io
import zlib
import argparse
from glob import iglob


TEST_DIR = 'htsim-tests'


def check_ref(ref_file: io.BufferedRandom, executable: str, output: str, param_str: list[str],
	    update:bool = False, verbose: bool = False):
	stdout = None
	if not verbose:
		stdout = subprocess.DEVNULL

	try:
		rc = subprocess.call(['./' + executable] + param_str, stdout = stdout)
		if rc != 0:
			print(f'Test failed: Exit status {rc}\n')
			return

		with open(output, 'rb') as out_file:
			if update:
				compress = zlib.compressobj()
				for out_chunk in iter(lambda: out_file.read(4096), b''):
					ref_file.write(compress.compress(out_chunk))
				ref_file.write(compress.flush())
				ref_file.truncate(ref_file.tell())

				print('Updated ref successfully\n')
				return

			decompress = zlib.decompressobj()
			for ref_chunk in iter(lambda: ref_file.read(4096), b''):
				ref_chunk_d = decompress.decompress(decompress.unconsumed_tail + ref_chunk)
				out_chunk = out_file.read(len(ref_chunk_d))
				if ref_chunk_d != out_chunk:
					print(f'Test failed: output and ref differ\n')
					return
			if out_file.read(1) != b'':
				print('Test failed: output and ref differ\n')
				return

	except Exception as e:
		print('Test failed:')
		print(e)
		print()
		return

	print('Test passed\n')


def run_test(test_name: str, update: bool = False, verbose: bool = False):
	folder = TEST_DIR + '/' + test_name
	try:
		with open(folder + '/test_config.json') as config_file:
			test_opts = json.load(config_file)
	except Exception as e:
		print('Test ' + test_name + ': error reading config')
		print(e)
		print()
		return

	params = test_opts.get('params')
	if params != None and (not(isinstance(params, list)) or any(not isinstance(p, str) for p in params)):
		print(f'{test_name}: Invalid parameters in config file')
		return

	print('Test ' + test_name)
	print('Parameters: ' + str(params))
	print('=' * 32)
	executable = 'htsim_' + test_name
	output = test_opts.get('output')
	out_param = []
	if output == None:
		output = folder + '/' + test_name + '.out'
		out_param = ['-o', output]

	ref_iterator = iglob(f'{folder}/*.ref')
	ref = next(ref_iterator, None)
	if not ref:
		print('No configurations found')
		return

	while (ref):
		print('# Checking: ' + ref)
		try:
			with open(ref, 'r+b') as ref_file:
				first_line = ref_file.readline()

				param_values = json.loads(first_line)
				not_found = list(filter(lambda param: param not in param_values, params))
				if not_found:
					print(f'Invalid options, params {not_found} not found')
				else:
					param_list = []
					for param in params:
						param_val = param_values[param]
						if isinstance(param_val, bool) and param_val == True:
							param_list.append('-' + param)
						elif not(isinstance(param_val, bool)):
							param_list += ['-' + param, str(param_val)]
					param_list += out_param
					print(f'Configuration: ' + (' '.join(param_list) if param_list else 'None'))
					check_ref(ref_file, executable, output, param_list, update, verbose)

		except Exception as e:
			print("Error reading ref:")
			print(e)
			print()

		ref = next(ref_iterator, None)

	try:
		os.remove(output)
	except FileNotFoundError:
		pass


if __name__ == '__main__':
	parser = argparse.ArgumentParser('tests.py', description = 'Runs HTSim test suite')
	parser.add_argument('-v', '--verbose', action = 'store_true')
	parser.add_argument('-u', '--update', action = 'store_true')
	args = parser.parse_args()

	try:
		all_tests = os.listdir(TEST_DIR)
	except Exception as e:
		print(e)
		exit(1)

	for test in all_tests:
		run_test(test, update = args.update, verbose = args.verbose)

