const { useState, createElement: e, useEffect, useRef, useLayoutEffect } = React;
const { Typography, Box, Button, TextField, Toolbar, IconButton, makeStyles } = MaterialUI;

const useStyles = makeStyles((theme) => ({
  toolbar: {
    display: "flex",
    justifyContent: "space-between",
    marginBottom: theme.spacing(2),
    alignItems: "center",
  },
  pipelineName: {
    fontWeight: "bold",
    fontSize: "1.5rem",
    marginLeft: theme.spacing(1),
  },
}));

export function PipelineCanvas({ pipeline, onBackButtonClick }) {
  const classes = useStyles();
  const [pipelineState, setPipelineState] = useState(pipeline.state);

  const handleButtonClick = async (id, action) => {
    const response = await fetch(`/api/v1/pipelines/${id}/${action}`, {
      method: "GET",
    });
    // Handle response if necessary
  };

  const fetchPipelineState = async () => {
    try {
      const response = await fetch(`/api/v1/pipelines/${pipeline.id}`);
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      const jsonData = await response.json();
      setPipelineState(jsonData.state);
    } catch (error) {
      console.error('Error fetching pipeline state:', error);
    }
  };

  useEffect(() => {
    fetchPipelineState(); // Fetch the pipeline state initially
    const interval = setInterval(fetchPipelineState, 1000); // Fetch the pipeline state every 1000ms
    return () => clearInterval(interval); // Cleanup the interval on component unmount
  }, []);

  return e(
    React.Fragment,
    null,
    e(
      Toolbar,
      { className: classes.toolbar },
      e(
        IconButton,
        {
          edge: "start",
          color: "inherit",
          onClick: onBackButtonClick,
        },
        e("i", { className: "material-icons" }, "arrow_back")
      ),
      e(
        Typography,
        {
          className: classes.pipelineName,
          style: { flexGrow: 1 },
        },
        pipeline.name
      ),
      e(
        "div",
        null,
        e(Button, {
          variant: "contained",
          style: {
            backgroundColor: pipelineState === "running" ? "red" : "green",
            color: "white",
          },
          onClick: () => handleButtonClick(pipeline.id, pipelineState === "running" ? "stop" : "start"),
        }, pipelineState === "running" ? e("i", { className: "material-icons" }, "stop") : e("i", { className: "material-icons" }, "play_arrow"))
      )
    ),
    e(
      "div",
      { style: { width: "100%", height: "calc(100% - 64px)", position: "relative" } },
      "Add a canvas here."
    )
  );
}
