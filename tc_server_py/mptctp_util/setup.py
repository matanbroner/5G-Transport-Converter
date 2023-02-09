from distutils.core import setup, Extension

mptcp_util = Extension('mptcp_util',
                       sources=['mptcp_util.c'])

setup(name='mptcp_util',
      version='1.0',
      description='MPTCP Utility functions',
      ext_modules=[mptcp_util])
