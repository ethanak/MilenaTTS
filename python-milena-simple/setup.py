from distutils.core import setup, Extension

module1 = Extension('milenasimple',
                    sources = ['milenasimple.c'],
                    libraries = ['milena','milena_mbrola'])


setup (name = 'python-milena',
       version = '0.3.1',
       description = 'Milena TTS Python interface',
       ext_modules = [module1]
       ,packages = ["milena"]
       )
