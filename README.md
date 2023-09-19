<!-- Improved compatibility of back to top link: See: https://github.com/Federicoand98/PathTracing/pull/73 -->
<a name="readme-top"></a>
<!--
*** Thanks for checking out the PathTracing. If you have a suggestion
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
  <h3 align="center">PathTracing in OpenGL</h3>

  <p align="center">
    A GPU-based PathTracing renderer for creating images with global illumination, advanced reflections, and physically-based materials.
    <br />
    <a href="https://github.com/Federicoand98/PathTracing"><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href="https://github.com/Federicoand98/PathTracing">View Demo</a>
    ·
    <a href="https://github.com/Federicoand98/PathTracing/issues">Report Bug</a>
    ·
    <a href="https://github.com/Federicoand98/PathTracing/issues">Request Feature</a>
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
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>



<!-- ABOUT THE PROJECT -->
## About The Project

This project is a full GPU-based RayTracing with PathTracing Renderer written in C++ and OpenGL 4.3.

![Product Name Screen Shot][product-screenshot]

This PathTracer project utilizes GPU acceleration through OpenGL Compute Shader to render various geometric shapes such as spheres, squares, triangles, cubes, and meshes.
It employs global illumination and a BSDF (Bidirectional Scattering Distribution Function) lighting model, incorporating features like Russian roulette for ray termination, 
optimized ray-object intersections using Bounding Boxes and Axis-Aligned Bounding Boxes (AABB), and the Fresnel-Schlick approximation.

This project aims to provide a fast and efficient path tracing solution for realistic rendering of 3D scenes.

<p align="right">(<a href="#readme-top">back to top</a>)</p>



### Built With

This section should list any major frameworks/libraries used to bootstrap your project. Leave any add-ons/plugins for the acknowledgements section. Here are a few examples.

* [GLEW][glew-url]
* [GLFW][glfw-url]
* [GLM][glm-url]
* [Dear ImGui][imgui-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Some Renders
![scene-1][scene-1]
![scene-2][scene-2]
![cb][cb]
![cb-2][cb-2]
![app-1][app-1]


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

Distributed under the Apache License. See `LICENSE.txt` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Federico Andrucci - federico.andrucci@gmail.com
Alex Gianelli - djgiane@yahoo.it

Project Link: [https://github.com/Federicoand98/PathTracing](https://github.com/Federicoand98/PathTracing)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## References

Use this space to list resources you find helpful and would like to give credit to. I've included a few of my favorites to kick things off!

* [OpenGL Compute Shaders](https://learnopengl.com/Guest-Articles/2022/Compute-Shaders/Introduction)
* [RayTracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
* [BSDF](https://blog.demofox.org/2020/06/14/casual-shadertoy-path-tracing-3-fresnel-rough-refraction-absorption-orbit-camera/)

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/Federicoand98/PathTracing.svg?style=for-the-badge
[contributors-url]: https://github.com/Federicoand98/PathTracing/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/Federicoand98/PathTracing.svg?style=for-the-badge
[forks-url]: https://github.com/Federicoand98/PathTracing/network/members
[stars-shield]: https://img.shields.io/github/stars/Federicoand98/PathTracing.svg?style=for-the-badge
[stars-url]: https://github.com/Federicoand98/PathTracing/stargazers
[issues-shield]: https://img.shields.io/github/issues/Federicoand98/PathTracing.svg?style=for-the-badge
[issues-url]: https://github.com/Federicoand98/PathTracing/issues
[license-shield]: https://img.shields.io/github/license/Federicoand98/PathTracing.svg?style=for-the-badge
[license-url]: https://github.com/Federicoand98/PathTracing/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/Federicoand98
[imgui-url]: https://github.com/ocornut/imgui
[glew-url]: https://glew.sourceforge.net/
[glfw-url]: https://www.glfw.org/
[glm-url]: https://github.com/g-truc/glm
[product-screenshot]: README/app-2.png
[app-1]: README/app-1.png
[cb]: README/cb.png
[cb-2]: README/cb-2.png
[scene-1]: README/scene-1.png
[scene-2]: README/scene-2.png
