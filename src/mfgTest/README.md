# Manufacturing Tests

After running the manufacturing test the results will be printed in a readable format to the CLI and at the end generate a JSON aggregating all the manufacturing tests' results. (Note that the cellular test can take up to 3 minutes to complete so it may appear as if it is stuck for a period of time).

Currently the only way to retrieve the JSON is to just copy it directly from the CLI and paste it into your preferred text editor. VSCode can automatically format JSON files into a readable format, to do so do the following:
1. ensure the file you are pasting to has the `.json` extension
2. make sure that the language mode is set to JSON, VSCode usually will automatically detect this but if not then go to the command prompt, select `Change Language Mode` and then type and select `JSON`
3. press `Shift + Alt + F` or right click and select `Format Document` to format the JSON to a readable format