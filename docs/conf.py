import subprocess, os

def configureDoxyfile(input_dir, output_dir):
    with open('Doxyfile.in', 'r') as file:
        filedata = file.read()

    filedata = filedata.replace('@DOXYGEN_INPUT_DIR@', input_dir)
    filedata = filedata.replace('@DOXYGEN_OUTPUT_DIR@', output_dir)

    with open('Doxyfile', 'w') as file:
        file.write(filedata)

read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

breathe_projects = {}
if read_the_docs_build:
    input_dir = '../include/beast'
    output_dir = 'build'
    configureDoxyfile(input_dir, output_dir)
    subprocess.call('doxygen', shell=True)
    breathe_projects['BEAST'] = output_dir + '/xml'

project = 'BEAST'
copyright = '2022, Jan Winkler'
author = 'Jan Winkler'
master_doc = 'index'

extensions = ['breathe']
templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
html_theme = 'sphinx_rtd_theme'
breathe_default_project = 'BEAST'
