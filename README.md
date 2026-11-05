<!-- Improved compatibility of back to top link: See: https://github.com/Federicoand98/Lucerna/pull/73 -->
<a name="readme-top"></a>
<!--
*** Thanks for checking out Lucerna. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]



<!-- PROJECT LOGO -->
<br />
<div align="center">
  <h3 align="center">Lucerna</h3>

  <p align="center">
    A real-time GPU path tracer in Vulkan, with global illumination, sunlight, and physically based materials.
    <br />
    <a href="https://github.com/Federicoand98/Lucerna"><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href="https://github.com/Federicoand98/Lucerna">View Demo</a>
    ·
    <a href="https://github.com/Federicoand98/Lucerna/issues">Report Bug</a>
    ·
    <a href="https://github.com/Federicoand98/Lucerna/issues">Request Feature</a>
  </p>
</div>



<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#some-renders">Some Renders</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>



<!-- ABOUT THE PROJECT -->
## About The Project

Lucerna is a GPU path tracer written in C++ and Vulkan 1.3. It traces rays in hardware through `VK_KHR_ray_query`, from a single compute shader. The name is Latin for oil lamp. Up to version 0.2.0 the project was called PathTracing.

![Sponza courtyard lit by the sun][sponza-courtyard]

![The application][product-screenshot]

Up to version 0.1.3 the renderer ran on OpenGL 4.3 compute shaders, with a BVH built and traversed in software. Version 0.2.0 moved it to Vulkan: the driver now builds the acceleration structures, and on the Sponza test scene a frame went from 52.6 ms to 1.2 ms at 1280×720. The OpenGL version is kept under the `v0.1.3` tag and the `opengl` branch.

Lucerna currently has:
- Hardware ray tracing with `VK_KHR_ray_query`: one BLAS per mesh, a procedural BLAS for spheres and quads, and a TLAS rebuilt when the scene changes
- Monte Carlo unidirectional integrator with Russian roulette
- Next event estimation with multiple importance sampling on emissive spheres and quads, and on a sun with a soft angular disc
- GGX microfacet BSDF with visible-normal sampling, for diffuse, metal, glossy, emissive and glass materials
- Glass with Fresnel, total internal reflection and Beer-Lambert absorption, on spheres and meshes
- Temporal accumulation with reprojection, so the image keeps converging while the camera moves
- Edge-avoiding à-trous denoiser guided by albedo, normal and depth
- Thin-lens depth of field and a firefly clamp
- Albedo textures with mipmaps, plus procedural checker and Perlin noise materials
- Tangent-space normal maps, in OpenGL or DirectX convention
- Alpha-masked geometry (leaves, chains, fences) that lets camera rays and shadows through, while the rest of the scene stays on the opaque hardware path
- Wavefront OBJ meshes with MTL materials, and glTF 2.0 scenes
- Live link with Blender: an add-on exports glTF on save and the renderer reloads it
- Scenes saved to and loaded from JSON
- ACES tonemapping and exposure
- Picking, gizmos and a material editor: every scene element can be changed in real time from the GUI
- Debug views for albedo, normals, depth and accumulation history

<p align="right">(<a href="#readme-top">back to top</a>)</p>



### Built With

* [Vulkan][vulkan-url]
* [GLFW][glfw-url]
* [GLM][glm-url]
* [Dear ImGui][imgui-url] and [ImGuizmo][imguizmo-url]
* [cgltf][cgltf-url], [stb_image][stb-url], [nlohmann/json][json-url], [portable-file-dialogs][pfd-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- GETTING STARTED -->
## Getting Started

### Prerequisites

* A GPU and driver with Vulkan 1.3, `VK_KHR_ray_query`, `VK_KHR_acceleration_structure` and `VK_KHR_push_descriptor`
* CMake 3.8 or newer and a C++20 compiler
* The Vulkan headers and loader, `glslc` and GLFW 3

On Ubuntu:

```sh
sudo apt install build-essential cmake libvulkan-dev glslc libglfw3-dev
```

The build has been tested on Linux only.

### Installation

1. Clone the repo
   ```sh
   git clone https://github.com/Federicoand98/Lucerna.git
   cd Lucerna
   ```
2. Build. The shaders are compiled to SPIR-V as part of the build
   ```sh
   cmake -S PathTracing -B PathTracing/build
   cmake --build PathTracing/build
   ```
3. Run. The executable is written to `PathTracing/res`
   ```sh
   ./PathTracing/res/Lucerna
   ```

To enable the Vulkan validation layers in a Release build, set `PT_VALIDATION=1`.

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- ROADMAP -->
## Roadmap

- Point and spot lights, with glTF light import
- HDR environment maps with importance sampling
- Emissive triangles and power-based light selection
- Roughness, metallic and emissive texture maps
- A stronger denoiser

See the [open issues][issues-url] for the full list.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Some Renders
![Thorn bushes with alpha masks, and their shadows][sponza-leaves]
![A flowering vase in the sun][sponza-vase]

Earlier renders, from the OpenGL version:

![scene-1][scene-1]
![scene-2][scene-2]
![cb][cb]
![cb-2][cb-2]


<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#readme-top">back to top</a>)</p>


<!-- LICENSE -->
## License

Distributed under the Apache License 2.0. See `LICENSE` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Federico Andrucci - federico.andrucci@gmail.com

Alex Gianelli - djgiane@yahoo.it

Project Link: [https://github.com/Federicoand98/Lucerna](https://github.com/Federicoand98/Lucerna)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## References

Use this space to list resources you find helpful and would like to give credit to. I've included a few of my favorites to kick things off!

* [Vulkan ray query](https://docs.vulkan.org/spec/latest/chapters/raytraversal.html)
* [OpenGL Compute Shaders](https://learnopengl.com/Guest-Articles/2022/Compute-Shaders/Introduction), used up to version 0.1.3
* [RayTracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
* [BSDF](https://blog.demofox.org/2020/06/14/casual-shadertoy-path-tracing-3-fresnel-rough-refraction-absorption-orbit-camera/)

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/Federicoand98/Lucerna.svg?style=for-the-badge
[contributors-url]: https://github.com/Federicoand98/Lucerna/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/Federicoand98/Lucerna.svg?style=for-the-badge
[forks-url]: https://github.com/Federicoand98/Lucerna/network/members
[stars-shield]: https://img.shields.io/github/stars/Federicoand98/Lucerna.svg?style=for-the-badge
[stars-url]: https://github.com/Federicoand98/Lucerna/stargazers
[issues-shield]: https://img.shields.io/github/issues/Federicoand98/Lucerna.svg?style=for-the-badge
[issues-url]: https://github.com/Federicoand98/Lucerna/issues
[license-shield]: https://img.shields.io/github/license/Federicoand98/Lucerna.svg?style=for-the-badge
[license-url]: https://github.com/Federicoand98/Lucerna/blob/master/LICENSE
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/Federicoand98
[imgui-url]: https://github.com/ocornut/imgui
[vulkan-url]: https://www.vulkan.org/
[imguizmo-url]: https://github.com/CedricGuillemet/ImGuizmo
[cgltf-url]: https://github.com/jkuhlmann/cgltf
[stb-url]: https://github.com/nothings/stb
[json-url]: https://github.com/nlohmann/json
[pfd-url]: https://github.com/samhocevar/portable-file-dialogs
[glfw-url]: https://www.glfw.org/
[glm-url]: https://github.com/g-truc/glm
[product-screenshot]: README/app-2.png
[sponza-courtyard]: README/sponza-courtyard.jpg
[sponza-leaves]: README/sponza-leaves.jpg
[sponza-vase]: README/sponza-vase.jpg
[cb]: README/cb.png
[cb-2]: README/cb-2.png
[scene-1]: README/scene-1.png
[scene-2]: README/scene-2.png
