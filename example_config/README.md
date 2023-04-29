# Example configuration files for different editors

## Folder structure
```
example_config/
├── vscode/
│   ├── launch.json
│   └── settings.json
└── README.md
```

## VSCode config files
To use the VSCode config files crate a .vscode folder in the `Lumen` folder and copy the `launch.json` and `settings.json` files over.

These settings are such that there will be one folder for each cmake build variant (eg. `Debug`, `Release` etc.). Further the `launch.json` file contains a launch target which launches the currently selected cmake build variant in debug mode, as well as setting the command line parameters to load the caustics example scene. Further the working directory for the executable is set to the project source directory to avoid loading errors (If this is not set, the assets will not be found by the executable).

Note that starting the executable in the folder where they are built will cause the program to crash as the assets are not found.

The final folder structure for VSCode should then look like this:

```
Lumen/
├── .vscode/
│   ├── launch.json
│   └── settings.json
├── assets/
├── example_config/
├── libs/
├── media/
├── scenes/
├── src/
...
└── README.md
```
