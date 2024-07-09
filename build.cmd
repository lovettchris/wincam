call src/build.cmd
python -m build  --outdir dist
python -m twine upload --repository pypi dist/*
