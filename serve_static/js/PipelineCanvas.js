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

  const handleButtonClick = (action) => {
    setPipelineState(action === "start" ? "running" : "stopped");
  };

  return e(
    React.Fragment,
    null,
    e(
      Toolbar,
      { className: classes.toolbar },
      e(
        Box,
        { display: "flex", alignItems: "center" },
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
          { className: classes.pipelineName },
          pipeline.name
        )
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
          onClick: () => handleButtonClick(pipelineState === "running" ? "stop" : "start"),
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

