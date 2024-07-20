pushd %~dp0\src
call build.cmd
if ERRORLEVEL 1 goto :eof
popd
if ERRORLEVEL 1 goto :eof
python -m build  --outdir dist
if ERRORLEVEL 1 goto :eof
python -m twine upload --repository pypi dist/*
