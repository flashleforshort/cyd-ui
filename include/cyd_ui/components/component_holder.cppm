// Copyright (c) 2024, Víctor Castillo Agüero.
// SPDX-License-Identifier: GPL-3.0-or-later

export module cydui.components:holder;

import std;
export import :with_template;

export
{
  namespace cyd::ui::components {
    struct component_builder_t {

      component_builder_t() = default;

      template<ComponentConcept C>
      component_builder_t(C comp) {
        components.emplace_back("", [=] {
          return std::shared_ptr<component_base_t> {new C {comp}};
        });

        //components[""] = std::make_unique<C>(comp);
      }

      template<typename T>
      component_builder_t(::with<T> _with) {
        for (const auto &item: _with.get_selection()) {
          components.emplace_back(item);
        }
      }

      std::vector<std::pair<std::string, std::shared_ptr<component_base_t>>> get_components() const {
        std::vector<std::pair<std::string, std::shared_ptr<component_base_t>>> _components { };
        for (const auto &item: components) {
          const auto &[id, builder] = item;
          _components.emplace_back(id, builder());
        }
        return _components;
      }

      const auto &get_component_constructors() const {
        return components;
      }

      void append_component(std::string id, const std::function<std::shared_ptr<component_base_t>()>& component) {
        components.emplace_back(id, component);
      }

      void transform(auto&& fun) {
        for (auto &item: components) {
          auto &[id, builder] = item;
          auto original_builder = builder;
          builder = [=]() {
            return fun(original_builder());
          };
        }
      }

    private:
      std::vector<std::pair<std::string, std::function<std::shared_ptr<component_base_t>()>>> components { };
    };

    struct component_holder_t {
      //template<ComponentConcept C>
      //inline component_holder_t(C &&comp) {
      //  components[""] = new C {comp};
      //  //components[""] = std::make_unique<C>(comp);
      //}
      //
      //template<ComponentConcept C>
      //inline component_holder_t(C &comp) {
      //  components[""] = new C {comp};
      //  //components[""] = std::make_unique<C>(comp);
      //}
      component_holder_t() = default;

      template<ComponentConcept C>
      component_holder_t(std::vector<C> comps) noexcept {
        std::size_t index = 0;
        for (auto &comp: comps) {
          components.emplace_back(std::to_string(index), std::shared_ptr<component_base_t> {new C {comp}});
          ++index;
        }
      }

      template<ComponentConcept C>
      component_holder_t(C comp) {
        components.emplace_back("", std::shared_ptr<component_base_t> {new C {comp}});
        //components[""] = std::make_unique<C>(comp);
      }

      template<typename T>
      component_holder_t(::with<T> _with) {
        for (const auto &item: _with.get_selection()) {
          components.emplace_back(item.first, item.second());
        }
      }

      component_holder_t(const component_builder_t &builder) {
        for (const auto &item: builder.get_components()) {
          components.emplace_back(item);
        }
      }

      component_holder_t(
        const std::vector<std::pair<std::string, std::shared_ptr<component_base_t>>> &components_
      ): components(components_) {
      }

      [[nodiscard]] auto &get_components() {
        return components;
      }

      [[nodiscard]] const auto &get_components() const {
        return components;
      }

      // Cannot work, since we would need to recursively copy any `component_holder_t` in the props
      // of each component.
      //component_holder_t clone() const {
      //
      //}

      void append_component(std::string id, std::shared_ptr<component_base_t> component) {
        components.emplace_back(id, component);
      }

      auto begin() {
        return components.begin();
      }

      auto begin() const {
        return components.begin();
      }

      auto end() {
        return components.end();
      }

      auto end() const {
        return components.end();
      }

    private:
      std::vector<std::pair<std::string, std::shared_ptr<component_base_t>>> components { };
    };
  }
}
