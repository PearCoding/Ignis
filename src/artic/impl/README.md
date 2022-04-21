The artic files in this directory will not be used in the precompiled driver and only in the JIT compiled shaders.

They contain implementation of various bsdfs, lights, cameras, etc...

Keep in mind that with apicollector you still have to use `make` or `ninja` to commit the changes in the directory to all the multiple executables.

Using `--script-dir` with the frontend is feasible workaround.