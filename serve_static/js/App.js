const { AppBar, Drawer, Fab, Dialog, DialogActions, DialogContent, DialogContentText, DialogTitle, TextField, Snackbar, List, ListItem, ListItemIcon, ListItemText, makeStyles, Toolbar, Typography, IconButton, Button, Divider } = MaterialUI;
const { render } = ReactDOM;
const { useState, useEffect, createElement: e } = React;
import { PipelineList } from './PipelineList.js';
import { StateContext } from './context.js';

const drawerWidth = 240;

const useStyles = makeStyles((theme) => ({
  root: { display: 'flex' },
  appBar: { zIndex: 1 },
  drawer: { width: drawerWidth, flexShrink: 0 },
  drawerPaper: { width: drawerWidth },
  content: { flexGrow: 1, padding: 20, marginTop: 50 },
  connectionStatus: {
    flexGrow: 1,
    textAlign: "center",
    position: "absolute",
    top: 10,
    right: 10,
    width: 150,
    height: 30,
    paddingTop: 10,
    border: "1px solid black",
  },
  connected: { backgroundColor: "green" },
  disconnected: { backgroundColor: "red" },
}));

export default function App() {
  const classes = useStyles();
  const [content, setContent] = useState("Welcome Home!");
  const [title, setTitle] = useState("Home");
  const [status, setStatus] = useState([]);
  const [error, setError] = useState(null);
  const [connected, setConnected] = useState(false);
  const [selectedDrawerItem, setSelectedDrawerItem] = useState(null);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('/api/v1/status');
        if (!response.ok) {
          throw new Error('Network response was not ok');
          setConnected(false);
        }
        setConnected(true);
        try {
          const jsonData = await response.json();
          // Process the returned data
          setStatus(jsonData);
          setError(null); // Reset error state if successful
        } catch (error) {
          console.log("Json error");
          setError(error.message); // Set error state if unsuccessful
        }
      } catch (error) {
        setConnected(false);
        setError(error.message); // Set error state if unsuccessful
      }
    };
    fetchData();
    const interval = setInterval(() => {
      fetchData();
    }, 1000); // Fetch data every 5 seconds
    return () => clearInterval(interval);
  }, []);

  const handlePipelinesClick = () => {
    setTitle('Pipelines');
    setContent(e(PipelineList));
  };

  const handleProgramCollectionClick = () => {
    setTitle('Program Collection');
    setContent('You have no program collection');
  };

  return (
    e(StateContext.Provider,
      { value: { connected, setConnected } },
      e("div", { className: classes.root },
        e(AppBar, { position: "fixed", className: classes.appBar },
          e(Toolbar, { style: { marginLeft: `${drawerWidth + 10}px` } },
            e(Typography, { variant: "h6" }, title),
            e(Typography, { variant: "h8",
                            className: classes.connectionStatus + " " +
                            (connected ? classes.connected : classes.disconnected)},
              connected ? ('Connected (v' + status["version"] + ')') : 'Disconnected')
           )
         ),
        e(Drawer, {
          className: classes.drawer,
          variant: "permanent",
          classes: {
            paper: classes.drawerPaper,
          }
        },
          e(List, null,
            e(
              'div',
              { style: { display: 'flex', justifyContent: 'center', alignItems: 'center', width: '100%' } },
              e("img", { src: "img/beast_head_logo_small.png", alt: "The BEAST logo", style: { width: '80%', objectFit: 'cover' }}),
            ),
            e(Divider),
            e(ListItem, {
              button: true,
              selected: selectedDrawerItem == "pipelines",
              onClick: () => {
                handlePipelinesClick();
                setSelectedDrawerItem("pipelines");
              },
            },
              e(ListItemIcon, null,
                e("i", { className: "material-icons" }, "lan" )
               ),
              e(ListItemText, { primary: "Pipelines" })
             ),
            e(ListItem, {
              button: true,
              selected: selectedDrawerItem == "program-collection",
              onClick: () => {
                handleProgramCollectionClick();
                setSelectedDrawerItem("program-collection");
              },
            },
              e(ListItemIcon, null,
                e("i", { className: "material-icons" }, "hive")
               ),
              e(ListItemText, { primary: "Program Collection" })
             )
           )
         ),
        e("main", { className: classes.content },
          e("p", null, content)
         )
       )
     )
  );
}
