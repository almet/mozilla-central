/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view displays the properties of objects.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

var gPane = null;
var gTab = null;
var gDebugger = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    testFrameParameters();
  });
}

function testFrameParameters()
{
  dump("Started testFrameParameters!\n");

  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    dump("Entered Debugger:FetchedVariables!\n");

    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      dump("After currentThread.dispatch!\n");

      var frames = gDebugger.DebuggerView.StackFrames._frames,
          localScope = gDebugger.DebuggerView.Properties.localScope,
          localNodes = localScope.querySelector(".details").childNodes;

      dump("Got our variables:\n");
      dump("frames     - " + frames.constructor + "\n");
      dump("localScope - " + localScope.constructor + "\n");
      dump("localNodes - " + localNodes.constructor + "\n");

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length, 11,
        "The localScope should contain all the created variable elements.");

      is(localNodes[0].querySelector(".info").textContent, "[object Proxy]",
        "Should have the right property value for 'this'.");

      // Expand the '__proto__', 'arguments' and 'a' tree nodes. This causes
      // their properties to be retrieved and displayed.
      localNodes[0].expand();
      localNodes[9].expand();
      localNodes[10].expand();

      // Poll every few milliseconds until the properties are retrieved.
      // It's important to set the timer in the chrome window, because the
      // content window timers are disabled while the debuggee is paused.
      let count = 0;
      let intervalID = window.setInterval(function(){
        if (++count > 50) {
          ok(false, "Timed out while polling for the properties.");
          resumeAndFinish();
        }
        if (!localNodes[0].fetched ||
            !localNodes[9].fetched ||
            !localNodes[10].fetched) {
          return;
        }
        window.clearInterval(intervalID);
        is(localNodes[0].querySelector(".property > .title > .key")
                        .textContent, "__proto__ ",
          "Should have the right property name for __proto__.");

        ok(localNodes[0].querySelector(".property > .title > .value")
                        .textContent.search(/object/) != -1,
          "__proto__ should be an object.");

        is(localNodes[9].querySelector(".info").textContent, "[object Object]",
          "Should have the right property value for 'c'.");

        is(localNodes[9].querySelectorAll(".property > .title > .key")[1]
                        .textContent, "a",
          "Should have the right property name for 'a'.");

        is(localNodes[9].querySelectorAll(".property > .title > .value")[1]
                        .textContent, 1,
          "Should have the right value for 'c.a'.");

        is(localNodes[10].querySelector(".info").textContent,
          "[object Arguments]",
          "Should have the right property value for 'arguments'.");

        is(localNodes[10].querySelector(".property > .title > .key")
                        .textContent, "length",
          "Should have the right property name for 'length'.");

        is(localNodes[10].querySelector(".property > .title > .value")
                        .textContent, 5,
          "Should have the right argument length.");

        resumeAndFinish();
      }, 100);
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  let thread = gDebugger.DebuggerController.activeThread;
  thread.addOneTimeListener("framescleared", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      var frames = gDebugger.DebuggerView.StackFrames._frames;

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames.");

      closeDebuggerAndFinish(gTab);
    }}, 0);
  });

  thread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
