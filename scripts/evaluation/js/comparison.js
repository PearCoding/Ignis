function initComparisons() {
    function compareImages(img) {
        var slider, img, clicked = 0, w, h;

        /* Get the width and height of the img element */
        w = img.firstElementChild.offsetWidth;
        h = img.firstElementChild.offsetHeight;
        /* Set the width of the img element to 50%: */
        img.style.width = (w / 2) + "px";

        /* Create slider if necessary: */
        slider = img.parentElement.getElementsByClassName("img-comp-slider")[0];
        if (!slider) {
            /* Create slider as we do not have one yet */
            slider = document.createElement("div");
            slider.setAttribute("class", "img-comp-slider");
            /* Insert slider */
            img.parentElement.insertBefore(slider, img);
        }

        /* Position the slider in the middle: */
        slider.style.top = "0px";
        slider.style.left = (w / 2) - (slider.offsetWidth / 2) + "px";
        /* Execute a function when the mouse button is pressed: */
        slider.addEventListener("mousedown", slideReady);
        /* And another function when the mouse button is released: */
        window.addEventListener("mouseup", slideFinish);
        /* Or touched (for touch screens: */
        slider.addEventListener("touchstart", slideReady);
        /* And released (for touch screens: */
        window.addEventListener("touchend", slideFinish);
        function slideReady(e) {
            /* Prevent any other actions that may occur when moving over the image: */
            e.preventDefault();
            /* The slider is now clicked and ready to move: */
            clicked = 1;
            /* Execute a function when the slider is moved: */
            window.addEventListener("mousemove", slideMove);
            window.addEventListener("touchmove", slideMove);
        }
        function slideFinish() {
            /* The slider is no longer clicked: */
            clicked = 0;
        }
        function slideMove(e) {
            var pos;
            /* If the slider is no longer clicked, exit this function: */
            if (clicked == 0) return false;
            /* Get the cursor's x position: */
            pos = getCursorPos(e)
            /* Prevent the slider from being positioned outside the image: */
            if (pos < 0) pos = 0;
            if (pos > w) pos = w;
            /* Execute a function that will resize the overlay image according to the cursor: */
            slide(pos);
        }
        function getCursorPos(e) {
            var a, x = 0;
            e = e || window.event;
            /* Get the x positions of the image: */
            a = img.getBoundingClientRect();
            /* Calculate the cursor's x coordinate, relative to the image: */
            x = e.pageX - a.left;
            /* Consider any page scrolling: */
            x = x - window.pageXOffset;
            return x;
        }
        function slide(x) {
            /* Resize the image: */
            img.style.width = x + "px";
            /* Position the slider: */
            slider.style.left = img.offsetWidth - (slider.offsetWidth / 2) + "px";
        }
    }

    /* Find all elements with an "overlay" class: */
    var x = document.getElementsByClassName("img-comp-overlay");
    for (var i = 0; i < x.length; i++) {
        /* Once for each "overlay" element:
        pass the "overlay" element as a parameter when executing the compareImages function: */
        compareImages(x[i]);
    }
}

function allowDrop(ev) {
    ev.preventDefault();
}

function drop(ev, is_left) {
    ev.preventDefault();
    var data = ev.dataTransfer.getData("text");

    // Update the method name labels (assumes they are the only sibling element)
    var method = data.split("/").slice(-1)[0];
    method = method.replace(".png", "").replace(".webp", "").replace(".jpg", "").replace(".jpeg", "");

    var method_name = METHODS_TO_NAME[method];
    if (method_name) {
        ev.target.src = data;
        if (is_left) {
            document.getElementsByClassName("left")[0].innerHTML = METHODS_TO_NAME[method];
        } else {
            document.getElementsByClassName("right")[0].innerHTML = METHODS_TO_NAME[method];
        }
    }
}