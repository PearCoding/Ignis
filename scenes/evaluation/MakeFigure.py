#!/bin/python3

import simpleimageio as sio
import figuregen
from figuregen.util.templates import FullSizeWithCrops
from figuregen.util.image import Cropbox
import sys
import os
import numpy as np

class Figure(FullSizeWithCrops):
    def __init__(self, exposure, *args, **kwargs):
        self.exposure = exposure
        super().__init__(*args, **kwargs)

    def tonemap(self, img):
        return figuregen.JPEG(sio.lin_to_srgb(sio.exposure(img, self.exposure)), quality=80)

    def compute_error(self, reference_image, method_image):
        return sio.relative_mse_outlier_rejection(method_image, reference_image, 0.001)
        # return sio.mse(method_image, reference_image)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        result_image = "./cbox4096.exr"
    else:
        result_image = sys.argv[1]

    script_dir = os.path.abspath(os.path.dirname(__file__))

    title = figuregen.Grid(1, 1)
    title.get_element(0, 0).set_image(figuregen.PNG(np.tile([255,255,255], (1, 500))))
    title.set_title("top", "Cornell Box - Comparison")

    ref_img = sio.read(f"{script_dir}/reference4096.exr")
    (h, w, _) = ref_img.shape
    
    figure = Figure(1,
        reference_image=ref_img,
        method_images=[
            sio.read(f"{result_image}"),
        ],
        crops=[
            Cropbox(top=20, left=100, height=96, width=128, scale=5),
            Cropbox(top=120, left=50, height=96, width=128, scale=5),
        ],
        method_names=["Reference", "Ignis"]
    )

    rows = figure.figure
    rows.insert(0, [title])

    # Generate the figure with the pdflatex backend and default settings
    figuregen.figure(rows, width_cm=17.7, filename="CornellBox.pdf")