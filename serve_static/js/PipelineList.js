const { AppBar, Drawer, Fab, Dialog, DialogActions, DialogContent, DialogContentText, DialogTitle, TextField, Snackbar, List, ListItem, ListItemIcon, ListItemText, makeStyles, Toolbar, Typography, IconButton, Button, Divider } = MaterialUI;
const { useState, useEffect, createElement: e } = React;

export function PipelineList() {
  const [pipelines, setPipelines] = useState([]);
  const [openDialog, setOpenDialog] = useState(false);
  const [pipelineName, setPipelineName] = useState("");
  const [snackbarOpen, setSnackbarOpen] = useState(false);
  const [snackbarMessage, setSnackbarMessage] = useState("");
  const [snackbarSeverity, setSnackbarSeverity] = useState("success");

  const handleOpenDialog = () => {
    setOpenDialog(true);
  };

  const handleCloseDialog = () => {
    setOpenDialog(false);
  };

  const handleSaveDialog = async () => {
    try {
      const response = await fetch("/api/v1/pipelines/new", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ name: pipelineName }),
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
          setError(error.message); // Set error state if unsuccessful
        }
      } catch (error) {
        setPipelines([]);
        setError(error.message); // Set error state if unsuccessful
      }
    };
    fetchData();
    const interval = setInterval(fetchData, 1000);
    return () => clearInterval(interval);
  }, []);

  function showPipelineDetails(id) {
    alert(`Show details for pipeline ID: ${id}`);
  }

  async function handleButtonClick(id, action) {
    const response = await fetch(`/api/v1/pipelines/${id}/${action}`, {
      method: "GET",
    });
    // Handle response if necessary
  }

  return e(
    React.Fragment,
    null,
    e(
      Typography,
      { variant: "body1", style: { marginBottom: 20 } },
      "Below is a list of available pipelines. Click on a pipeline to view its details, or use the Play/Stop buttons to control its execution."
    ),
    e(
      List,
      null,
      pipelines.map((pipeline) =>
                    e(
                      ListItem,
                      {
                        key: pipeline.id,
                        button: true,
                        onClick: () => showPipelineDetails(pipeline.id),
                      },
                      e(ListItemIcon, null, e("i", { className: "material-icons" }, "lan")),
                      e(ListItemText, { primary: pipeline.name }),
                      e(
                        Button,
                        {
                          variant: "contained",
                          style: {
                            backgroundColor: pipeline.state === "running" ? "red" : "green",
                            color: "white",
                          },
                          color: pipeline.state === "running" ? "secondary" : "primary",
                          onClick: (event) => {
                            event.stopPropagation();
                            handleButtonClick(pipeline.id, pipeline.state === "running" ? "stop" : "start");
                          },
                        },
                        pipeline.state === "running" ? e("i", { className: "material-icons" }, "stop") : e("i", { className: "material-icons" }, "play_arrow")
                      )
                    )
                   )
    ),
    pipelines.length === 0 &&
      e(
        Typography,
        { variant: "body1", style: { marginBottom: 20 } },
        "Click '+' to create a new pipeline."
      ),
    e(Fab, {
      color: "primary",
      style: { position: "fixed", bottom: 20, right: 20 },
      onClick: handleOpenDialog,
//      disabled: !connected,
    }, e("i", { className: "material-icons", style: { pointerEvents: "none" } }, "add")),
    e(
      Dialog,
      {
        open: openDialog,
        onClose: handleCloseDialog,
      },
      e(DialogTitle, null, "Create a New Pipeline"),
      e(DialogContent, null,
        e(DialogContentText, null, "Enter the name of the new pipeline."),
        e(TextField, {
          autoFocus: true,
          margin: "dense",
          label: "Pipeline Name",
          fullWidth: true,
          onChange: (event) => setPipelineName(event.target.value),
        })
       ),
      e(DialogActions, null,
        e(Button, { onClick: handleCloseDialog, color: "primary" }, "Cancel"),
        e(Button, { onClick: handleSaveDialog, color: "primary" }, "Save")
       )
    ),
    e(Snackbar, {
      open: snackbarOpen,
      autoHideDuration: 6000,
      onClose: handleSnackbarClose,
      anchorOrigin: { vertical: "bottom", horizontal: "left" },
      message: snackbarMessage,
    })
  );
}
