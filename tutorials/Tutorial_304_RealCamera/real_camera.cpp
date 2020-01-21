/**
 * Copyright (C) 2015 by Liangliang Nan (liangliang.nan@gmail.com)
 * https://3d.bk.tudelft.nl/liangliang/
 *
 * This file is part of Easy3D. If it is useful in your research/work,
 * I would be grateful if you show your appreciation by citing it:
 * ------------------------------------------------------------------
 *      Liangliang Nan.
 *      Easy3D: a lightweight, easy-to-use, and efficient C++
 *      library for processing and rendering 3D data. 2018.
 * ------------------------------------------------------------------
 * Easy3D is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3
 * as published by the Free Software Foundation.
 *
 * Easy3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "real_camera.h"

#include <easy3d/core/point_cloud.h>
#include <easy3d/viewer/drawable_points.h>
#include <easy3d/viewer/drawable_lines.h>
#include <easy3d/viewer/camera.h>
#include <easy3d/viewer/manipulated_camera_frame.h>
#include <easy3d/viewer/texture.h>
#include <easy3d/viewer/shader_manager.h>
#include <easy3d/viewer/shader_program.h>
#include <easy3d/viewer/primitives.h>
#include <easy3d/viewer/setting.h>
#include <easy3d/util/string.h>
#include <easy3d/util/file_system.h>

#include <3rd_party/glfw/include/GLFW/glfw3.h>	// for the KEYs


using namespace easy3d;


RealCamera::RealCamera(const std::string& title,
                       const std::string& bundler_file,
                       const std::string& cloud_file)
    : Viewer(title, 4, 3, 2, false, false)
    , current_view_(0)
    , texture_(nullptr)
{
    // Read the point cloud
    if (add_model(cloud_file)) {
        auto drawable = current_model()->points_drawable("vertices");
        drawable->set_per_vertex_color(true);
        drawable->set_point_size(5.0f);
        
        // Read the camera parameters from the bundler file.
        if (read_bundler_file(bundler_file))
            create_cameras_drawable();
        else
            std::cerr << "Error: failed load bundler file." << std::endl;

        camera()->setUpVector(vec3(0, 1, 0));
        camera()->setViewDirection(vec3(0, 0, -1));
        camera_->showEntireScene();
    }
    else
        std::cerr << "Error: failed load point cloud." << std::endl;
}


std::string RealCamera::usage() const {
    return ("------------ Real Camera usage ---------- \n"
            "Press 'Space' to switch views\n"
            "Press 'H' to show/hide the cameras\n"
            "----------------------------------------- \n");
}


bool RealCamera::key_press_event(int key, int modifiers) {
    if (key == GLFW_KEY_SPACE) {
        if (!views_.empty()) {
            current_view_ = (current_view_ + 1) % views_.size();
            if (KRT_to_camera(current_view_, 2, camera())) {
                std::cout << "----- view " << current_view_ << " ------" << std::endl;
                set_title("RealCamera: View_" + std::to_string(current_view_));
                const CameraPara& c = views_[current_view_];
                // make sure the aspect ratio (actual size does not matter)
                resize(c.w * 0.3, c.h * 0.3);
            }
        }
        return true;
    }
    else if (key == GLFW_KEY_1) {
        if (!views_.empty()) {
            if (KRT_to_camera(current_view_, 1, camera())) {
                std::cout << "----- view " << current_view_ << " ------" << std::endl;
                const CameraPara& c = views_[current_view_];
                // make sure the aspect ratio (actual size does not matter)
                resize(c.w * 0.3, c.h * 0.3);
                update();
            }
        }
        return true;
    }
    else if (key == GLFW_KEY_2) {
        if (!views_.empty()) {
            if (KRT_to_camera(current_view_, 2, camera())) {
                std::cout << "----- view " << current_view_ << " ------" << std::endl;
                const CameraPara& c = views_[current_view_];
                // make sure the aspect ratio (actual size does not matter)
                resize(c.w * 0.3, c.h * 0.3);
                update();
            }
        }
        return true;
    }
    else if (key == GLFW_KEY_H) {
        LinesDrawable* d = current_model()->lines_drawable("cameras");
        if (d) {
            d->set_visible(!d->is_visible());
            update();
        }
        return true;
    }
    else
        return Viewer::key_press_event(key, modifiers);
}


void RealCamera::load_image() {
    const std::string image_file = setting::resource_directory() + "/data/fountain/images/" + string::from_integer(current_view_, 4, '0') + ".jpg";
    if (file_system::is_file(image_file)) {
            if (texture_)
                delete texture_;
        texture_ = Texture::create(image_file);
    }
    update();
}


bool RealCamera::KRT_to_camera(std::size_t view_index, int method, Camera* c) {
    if (view_index < 0 || view_index >= views_.size()) {
        std::cerr << "Error: invalid view index (" << view_index << ")" << std::endl;
        return false;
    }
    
    const CameraPara& cam = views_[view_index];
    float fx = cam.fx;
    float fy = cam.fy;
    float cx = cam.cx;
    float cy = cam.cy;
    
    if (method == 1) {
        c->set_from_calibration(
                    fx, fy, 0.0f, cx, cy,
                    vec3(cam.rx, cam.ry, cam.rz),
                    vec3(cam.tx, cam.ty, cam.tz)
                    );
    }
    else if (method == 2) {
        const vec3 rot_vec(-cam.rx, -cam.ry, -cam.rz);
        const float angle = rot_vec.length();
        const quat q(rot_vec / angle, angle);
        c->setOrientation(q);

        const vec3 pos(cam.tx, cam.ty, cam.tz);
        c->setPosition(-q.rotate(pos));

        const float proj_5th = 2.0f * fy / cam.h;
        c->setFieldOfView(2.0f * atan(1.0f / proj_5th));
    }

    load_image();

	return true;
}


void RealCamera::create_cameras_drawable()
{    
    std::vector<vec3> vertices;
    for (std::size_t i = 0; i < views_.size(); ++i) {
        Camera c;
        KRT_to_camera(i, 1, &c);
        std::vector<vec3> points;
        opengl::prepare_camera(points, camera()->sceneRadius() * 0.03f, static_cast<float>(views_[i].h)/views_[i].w);
        const mat4& m = c.frame()->worldMatrix();
        for (auto& p : points) {
            vertices.push_back(m * p);
        }
    }
    LinesDrawable* d = current_model()->lines_drawable("cameras");
    if (!d)
        d = current_model()->add_lines_drawable("cameras");
    d->update_vertex_buffer(vertices);
    d->set_default_color(vec3(0, 0, 1));
    d->set_line_width(2.0f);
}


void RealCamera::draw() const {
    Viewer::draw();
    draw_image();
}


void RealCamera::draw_image() const {
    if (texture_ == nullptr)
        return;

    static const std::string quad_name = "screen_space/quad_color_texture";
    ShaderProgram* program = ShaderManager::get_program(quad_name);
    if (!program) {
        std::vector<ShaderProgram::Attribute> attributes = {
            ShaderProgram::Attribute(ShaderProgram::POSITION, "vertexMC"),
            ShaderProgram::Attribute(ShaderProgram::TEXCOORD, "tcoordMC")
        };
        program = ShaderManager::create_program_from_files(quad_name, attributes);
    }
    if (!program)
        return;

    int w = texture_->width();
    int h = texture_->height();
    float image_as = w / static_cast<float>(h);
    float viewer_as = width() / static_cast<float>(height());
    if (image_as < viewer_as) {// thin
        h = static_cast<int>(height() * 0.5f);
        w = static_cast<int>(h * image_as);
    }
    else {
        w = static_cast<int>(width() * 0.5f);
        h = static_cast<int>(w / image_as);
    }

    program->bind();
    program->bind_texture("textureID", texture_->id(), 0);
    opengl::draw_quad(ShaderProgram::POSITION, ShaderProgram::TEXCOORD, 0, 0, w, h, width(), height(), -0.9f);
    program->release_texture();
    program->release();
}
