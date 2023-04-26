# pipefx debugger
This project is used to finetune effects config for [pipefx](https://github.com/jacopomaroli/pipefx).  
At its current stage is meant to be run on windows and visual studio 2019

# setup
1. copy the following deps as symlinks into the `3rdparty` folder
 - boost_1_70_0 https://www.boost.org/users/history/version_1_70_0.html
 - pipefx https://github.com/jacopomaroli/pipefx
 - q https://github.com/cycfi/Q
 - websocketpp https://github.com/zaphoyd/websocketpp
2. copy `config.example.cfg` as `config.cfg`. Add a raw audio input file and adjust the settings
3. open pipefx_dbg.sln with visual studio 2019 compile and run
4. with a command prompt, get into the frontend folder and run `npm run` and `npm serve`.  
nodejs needs to be installed on your machine.  
You can serve the content of the public folder through any other means

# boost compile notes
tested with boost 1.70.0
Download boost 1.70.0 and run the following commands in a `x86_x64 Cross Tools Command Prompt for VS 2019` prompt.
`.\bootstrap.bat --without-python`
`b2 -j8 toolset=msvc-14.2 address-model=32,64 architecture=x86 link=static threading=multi runtime-link=shared --build-type=minimal stage --stagedir=stage --with-date_time --with-regex`
