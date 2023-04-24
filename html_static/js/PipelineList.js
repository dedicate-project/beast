const {
  AppBar,
  Drawer,
  Fab,
  Dialog,
  DialogActions,
  DialogContent,
  DialogContentText,
  DialogTitle,
  TextField,
  Snackbar,
  List,
  ListItem,
  ListItemIcon,
  ListItemText,
  makeStyles,
  Toolbar,
  Typography,
  IconButton,
  Button,
  Divider
} = MaterialUI;
const {useContext, useState, useEffect, createElement : e} = React;
import {StateContext} from './context.js';
import {PipelineCanvas} from './PipelineCanvas.js';
import {ConfirmationDialog} from './ConfirmationDialog.js';

export function PipelineList() {
  const {connected} = useContext(StateContext);

  const [pipelines, setPipelines] = useState([]);
  const [openDialog, setOpenDialog] = useState(false);
  const [pipelineName, setPipelineName] = useState("");
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMessage, setSnackbarMessage] = useState("");
  const [snackbarSeverity, setSnackbarSeverity] = useState("success");
  const [hoveredPipelineId, setHoveredPipelineId] = useState(null);
  const [renameDialogOpen, setRenameDialogOpen] = useState(false);
  const [renamingPipeline, setRenamingPipeline] = useState(null);
  const [deleteDialogOpen, setDeleteDialogOpen] = useState(false);
  const [deletingPipeline, setDeletingPipeline] = useState(null);

  const handleOpenDialog = () => { setOpenDialog(true); };

  const handleCloseDialog = () => { setOpenDialog(false); };

  const handleDeleteButtonClick = (pipeline) => {
    setDeletingPipeline(pipeline);
    setDeleteDialogOpen(true);
  };

  function showPipelineDetails(pipeline) { setSelectedPipeline(pipeline); }

  const handleOpenRenameDialog = (pipeline) => {
    setPipelineName(pipeline.name);
    setRenamingPipeline(pipeline);
    setRenameDialogOpen(true);
  };

  const handleDeletePipeline = async () => {
    try {
      const response = await fetch(`/api/v1/pipelines/${deletingPipeline.id}/delete`, {
        method : "GET",
      });
      // Handle response if necessary
    } catch (error) {
      // Handle error if necessary
    }
    setDeleteDialogOpen(false);
  };

  const handleCloseRenameDialog = () => { setRenameDialogOpen(false); };

  const handleSaveRenameDialog = async () => {
    try {
      const response = await fetch(`/api/v1/pipelines/${renamingPipeline.id}/update`, {
        method : "POST",
        headers : {
          "Content-Type" : "application/json",
        },
        body : JSON.stringify({action : "change_name", name : pipelineName}),
      });
      // Handle response if necessary
    } catch (error) {
      // Handle error if necessary
    }
    setRenameDialogOpen(false);
  };

  const [selectedPipeline, setSelectedPipeline] = useState(null);

  const handleSaveDialog = async () => {
    try {
      const response = await fetch("/api/v1/pipelines/new", {
        method : "POST",
        headers : {
          "Content-Type" : "application/json",
        },
        body : JSON.stringify({name : pipelineName}),
      });
      const jsonData = await response.json();
      if (jsonData.status === "success") {
        setSnackbarSeverity("success");
        setSnackbarMessage("Pipeline successfully created");
      } else {
        setSnackbarSeverity("error");
        setSnackbarMessage(`Pipeline creation failed: ${jsonData.error}`);
      }
    } catch (error) {
      setSnackbarSeverity("error");
      setSnackbarMessage(`Pipeline creation failed: ${error.message}`);
    }
    setSnackbarOpen(true);
    setOpenDialog(false);
  };

  const handleSnackbarClose = (event, reason) => {
    if (reason === "clickaway") {
      return;
    }
    setSnackbarOpen(false);
  };

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('/api/v1/pipelines');
        if (!response.ok) {
          throw new Error('Network response was not ok');
        }
        try {
          const jsonData = await response.json();
          // Process the returned data
          setPipelines(jsonData);
        } catch (error) {
          setPipelines([]);
          console.log("Json error");
        }
      } catch (error) {
        setPipelines([]);
      }
    };
    fetchData();
    const interval = setInterval(fetchData, 1000);
    return () => clearInterval(interval);
  }, []);

  async function handleButtonClick(id, action) {
    const response = await fetch(`/api/v1/pipelines/${id}/${action}`, {
      method : "GET",
    });
    // Handle response if necessary
  }

  return e(
      React.Fragment, null,
      selectedPipeline === null
          ? e(
                React.Fragment,
                null,
                e(Typography, {component : "div", variant : "body1", style : {marginBottom : 20}},
                  "Below is a list of available pipelines. Click on a pipeline to view its details, or use the Play/Stop buttons to control its execution."),
                e(List, null,
                  pipelines
                      .map((pipeline) => e(
                               ListItem,
                               {
                                 key : pipeline.id,
                                 button : true,
                                 onClick : () => showPipelineDetails(pipeline),
                                 onMouseEnter : () => setHoveredPipelineId(pipeline.id),
                                 onMouseLeave : () => setHoveredPipelineId(null),
                               },
                               e(ListItemIcon, null, e("i", {className : "material-icons"}, "lan")),
                               e(ListItemText, {
                                 primary : e(React.Fragment, null, pipeline.name,
                                             hoveredPipelineId === pipeline.id &&
                                                 e(IconButton, {
                                                   edge : "end",
                                                   color : "inherit",
                                                   onClick : (event) => {
                                                     event.stopPropagation();
                                                     handleOpenRenameDialog(pipeline);
                                                   },
                                                   style : {padding : 3, marginLeft : 4},
                                                 },
                                                   e("i", {
                                                     className : "material-icons",
                                                     style : {fontSize : 18}
                                                   },
                                                     "create"))),
                               }),
                               e(Button, {
                                 variant : "contained",
                                 style : {
                                   backgroundColor : pipeline.state === "running" ? "red" : "green",
                                   color : "white",
                                 },
                                 color : pipeline.state === "running" ? "secondary" : "primary",
                                 onClick : (event) => {
                                   event.stopPropagation();
                                   handleButtonClick(pipeline.id, pipeline.state === "running"
                                                                      ? "stop"
                                                                      : "start");
                                 },
                               },
                                 pipeline.state === "running"
                                     ? e("i", {className : "material-icons"}, "stop")
                                     : e("i", {className : "material-icons"}, "play_arrow")),
                               e(Button, {
                                 variant : "contained",
                                 style : {
                                   backgroundColor : "red",
                                   color : "white",
                                   marginLeft : 4,
                                 },
                                 color : "secondary",
                                 onClick : (event) => {
                                   event.stopPropagation();
                                   handleDeleteButtonClick(pipeline);
                                 },
                               },
                                 e("i", {className : "material-icons"}, "delete")),
                               ))),

                pipelines.length === 0 &&
                    e(Typography, {variant : "body1", style : {marginBottom : 20}},
                      "Click '+' to create a new pipeline."),
                e(Fab, {
                  color : "primary",
                  style : {position : "fixed", bottom : 20, right : 20},
                  onClick : handleOpenDialog,
                  disabled : !connected,
                },
                  e("i", {className : "material-icons", style : {pointerEvents : "none"}}, "add")),
                e(Dialog, {
                  open : openDialog,
                  onClose : handleCloseDialog,
                },
                  e(DialogTitle, null, "Create a New Pipeline"),
                  e(DialogContent, null,
                    e(DialogContentText, null, "Enter the name of the new pipeline."),
                    e(TextField, {
                      autoFocus : true,
                      margin : "dense",
                      label : "Pipeline Name",
                      fullWidth : true,
                      onChange : (event) => setPipelineName(event.target.value),
                    })),
                  e(
                      DialogActions, null,
                      e(Button, {onClick : handleCloseDialog, color : "primary"}, "Cancel"),
                      e(Button, {
                        onClick : handleSaveDialog,
                        color : "primary",
                        disabled : !pipelineName.trim()
                      },
                        "Save") // Disable the Save button if pipelineName is empty or contains
                                // only spaces
                      )),
                e(Dialog, {
                  open : renameDialogOpen,
                  onClose : handleCloseRenameDialog,
                },
                  e(DialogTitle, null, "Rename Pipeline"),
                  e(DialogContent, null,
                    e(DialogContentText, null, "Enter the new name for the pipeline."),
                    e(TextField, {
                      autoFocus : true,
                      margin : "dense",
                      label : "Pipeline Name",
                      fullWidth : true,
                      value : pipelineName,
                      onChange : (event) => setPipelineName(event.target.value),
                    })),
                  e(DialogActions, null,
                    e(Button, {onClick : handleCloseRenameDialog, color : "primary"}, "Cancel"),
                    e(Button, {
                      onClick : handleSaveRenameDialog,
                      color : "primary",
                      disabled : !pipelineName.trim() || pipelineName === renamingPipeline?.name,
                    },
                      "Save"))),
                e(ConfirmationDialog, {
                  open : deleteDialogOpen,
                  onClose : () => setDeleteDialogOpen(false),
                  onConfirm : handleDeletePipeline,
                  title : "Delete Pipeline",
                  content : "Are you sure you want to delete this pipeline?",
                }),
                )
          : e(PipelineCanvas, {
              pipeline : selectedPipeline,
              onBackButtonClick : () => setSelectedPipeline(null),
            }),
      e(Snackbar, {
        open : snackbarOpen,
        autoHideDuration : 6000,
        onClose : handleSnackbarClose,
        anchorOrigin : {vertical : "bottom", horizontal : "left"},
        message : snackbarMessage,
      }));
}
