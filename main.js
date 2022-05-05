const electron = require( 'electron' );
const { app, BrowserWindow } = electron;

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let win

function createWindow () {
  win = new BrowserWindow( { width: 800
                           , height: 600
                           , webPreferences: { nodeIntegration: true
                                             , webSecurity: false
                                             , contextIsolation: false
                                             , enableRemoteModule: true } } )

  // and load the index.html of the app.
  win.loadFile('index.html')

  win.webContents.openDevTools()

  // Emitted when the window is closed.
  win.on( 'closed', () => {
    win = null
  })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on( 'ready', createWindow )

// Quit when all windows are closed.
app.on( 'window-all-closed', () => {
  if( process.platform !== 'darwin' ) {
    app.quit()
  }
})

app.on( 'activate', () => {
  if( win === null ) {
    createWindow()
  }
})
